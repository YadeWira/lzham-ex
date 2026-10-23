#!/usr/bin/env python3
# Reference decoder for LZHAM 1.0 streams (LZHAM_DLL_VERSION 0x1010), written to document the format and to
# serve as an executable specification for third-party decoders (for example a ZPAQL implementation).
#
# Scope: the streams produced by lzham_compress_memory()/lzham_compress() with no seed dictionary and no
# zlib wrapper (method 14 streams are also accepted, the 2-byte header is skipped). It decodes blocks, raw
# blocks, sync blocks, the arithmetic-coded flags, the quasi-adaptive Huffman tables, rep matches, delta
# literals and huge matches, and checks the trailing Adler-32.
#
# Usage: python3 lzham_ref.py compressed.lzham output.bin [--dict 20] [--rate 8]
#
# LZHAM is in the Public Domain. See LICENSE.
import sys


class Bits:
    """MSB-first bit reader. Past the end of the buffer it returns zero bits (like the codec)."""

    def __init__(self, data):
        self.data = data
        self.pos = 0
        self.buf = 0
        self.n = 0

    def get(self, k):
        while self.n < k:
            c = self.data[self.pos] if self.pos < len(self.data) else 0
            self.pos += 1
            self.buf = (self.buf << 8) | c
            self.n += 8
        if k == 0:
            return 0
        self.n -= k
        v = (self.buf >> self.n) & ((1 << k) - 1)
        self.buf &= (1 << self.n) - 1
        return v

    def align(self):
        self.get(self.n & 7)


def moffat(a):
    """calculate_minimum_redundancy() by Moffat & Katajainen: in-place code lengths for sorted weights."""
    n = len(a)
    if n == 0:
        return a
    if n == 1:
        a[0] = 0
        return a
    a[0] += a[1]
    root, leaf = 0, 2
    for nxt in range(1, n - 1):
        if leaf >= n or a[root] < a[leaf]:
            a[nxt] = a[root]
            a[root] = nxt
            root += 1
        else:
            a[nxt] = a[leaf]
            leaf += 1
        if leaf >= n or (root < nxt and a[root] < a[leaf]):
            a[nxt] += a[root]
            a[root] = nxt
            root += 1
        else:
            a[nxt] += a[leaf]
            leaf += 1
    a[n - 2] = 0
    for nxt in range(n - 3, -1, -1):
        a[nxt] = a[a[nxt]] + 1
    avbl = 1
    used = dpth = 0
    root = n - 2
    nxt = n - 1
    while avbl > 0:
        while root >= 0 and a[root] == dpth:
            used += 1
            root -= 1
        while avbl > used:
            a[nxt] = dpth
            nxt -= 1
            avbl -= 1
        avbl = 2 * used
        dpth += 1
        used = 0
    return a


def huffman_code_sizes(freq):
    """generate_huffman_codes(): Moffat over frequencies sorted ascending (stable by symbol index)."""
    used = [i for i, f in enumerate(freq) if f]
    sizes = [0] * len(freq)
    if len(used) == 1:
        sizes[used[0]] = 1
        return sizes
    order = sorted(used, key=lambda i: freq[i])
    depths = moffat([freq[i] for i in order])
    for k, sym in enumerate(order):
        sizes[sym] = depths[k]
    return sizes


def limit_code_sizes(sizes, max_size=16):
    """prefix_coding::limit_max_code_size(): LHArc-style tree adjustment."""
    max_ever = 34
    num_codes = [0] * (max_ever + 1)
    should_limit = False
    for c in sizes:
        num_codes[c] += 1
        if c > max_size:
            should_limit = True
    if not should_limit:
        return sizes

    ofs = 0
    next_sorted_ofs = [0] * (max_ever + 1)
    for i in range(1, max_ever + 1):
        next_sorted_ofs[i] = ofs
        ofs += num_codes[i]
    if ofs < 2 or ofs > 1024:
        return sizes
    if ofs > (1 << max_size):
        raise ValueError("limit_max_code_size: too many symbols")

    for i in range(max_size + 1, max_ever + 1):
        num_codes[max_size] += num_codes[i]

    total = 0
    for i in range(max_size, 0, -1):
        total += num_codes[i] << (max_size - i)
    if total == (1 << max_size):
        return sizes

    while True:
        num_codes[max_size] -= 1
        i = max_size - 1
        while i:
            if num_codes[i]:
                num_codes[i] -= 1
                num_codes[i + 1] += 2
                break
            i -= 1
        if not i:
            raise ValueError("limit_max_code_size: cannot adjust")
        total -= 1
        if total == (1 << max_size):
            break

    new_sizes = []
    for i in range(1, max_size + 1):
        new_sizes += [i] * num_codes[i]

    out = list(sizes)
    for i, c in enumerate(sizes):
        if c:
            out[i] = new_sizes[next_sorted_ofs[c]]
            next_sorted_ofs[c] += 1
    return out


class Model:
    """raw_quasi_adaptive_huffman_data_model: implicit, symmetrically updated Huffman tables."""

    def __init__(self, num_syms, max_update_interval=64, adapt_rate=64):
        self.num_syms = num_syms
        self.max_update_interval = max_update_interval or 64
        self.adapt_rate = max(32, adapt_rate or 64)
        self.max_cycle = min((max(24, num_syms) + 6) * self.max_update_interval, 32767)
        self.reset()

    def reset(self):
        self.freq = [1] * self.num_syms
        self.update_cycle = self.num_syms
        self.total_count = 0
        self.symbols_until_update = 0
        self.update_tables(min(self.max_cycle, 16), all_ones=True)

    def rescale(self):
        self.freq = [(f + 1) >> 1 for f in self.freq]
        self.total_count = sum(self.freq)

    def update_tables(self, force_cycle=None, all_ones=False):
        self.total_count += self.update_cycle
        while self.total_count >= 32768:
            self.rescale()

        if all_ones and self.num_syms >= 2:
            # Shortcut for the initial all-ones table, exactly as the codec does it.
            base = self.num_syms.bit_length() - 1
            num_left = (self.num_syms - (1 << base)) * 2
            if num_left > self.num_syms:
                num_left = self.num_syms
            sizes = [base + 1] * num_left + [base] * (self.num_syms - num_left)
        else:
            sizes = huffman_code_sizes(self.freq)
            if max(sizes) > 16:
                sizes = limit_code_sizes(sizes, 16)

        if force_cycle is not None:
            self.symbols_until_update = self.update_cycle = force_cycle
        else:
            self.update_cycle = (31 + self.update_cycle * self.adapt_rate) >> 5
            if self.update_cycle > self.max_cycle:
                self.update_cycle = self.max_cycle
            self.symbols_until_update = self.update_cycle

        self.build(sizes)

    def reset_update_rate(self):
        self.total_count += self.update_cycle - self.symbols_until_update
        if self.total_count > self.num_syms:
            self.rescale()
        self.symbols_until_update = self.update_cycle = min(8, self.update_cycle)

    def build(self, sizes):
        # Canonical codes: by code length, and by symbol index inside each length.
        self.sizes = sizes
        self.table = {}
        code = 0
        for length in range(1, 17):
            for sym in range(self.num_syms):
                if sizes[sym] == length:
                    self.table[(length, code)] = sym
                    code += 1
            code <<= 1

    def decode(self, bits):
        code = 0
        sym = None
        for length in range(1, 17):
            code = (code << 1) | bits.get(1)
            sym = self.table.get((length, code))
            if sym is not None:
                break
        if sym is None:
            raise ValueError("bad Huffman code")

        self.freq[sym] += 1
        self.symbols_until_update -= 1
        if self.symbols_until_update == 0:
            self.update_tables()
        return sym


class Arithmetic:
    """The codec's binary arithmetic decoder (11-bit probabilities, adaptation shift 5)."""

    MIN_LEN = 0x01000000

    def __init__(self, bits):
        self.bits = bits
        self.value = 0
        self.length = 0

    def start(self):
        self.value = 0
        for _ in range(4):
            self.value = ((self.value << 8) | self.bits.get(8)) & 0xFFFFFFFF
        self.length = 0xFFFFFFFF

    def bit(self, probs, index):
        while self.length < self.MIN_LEN:
            self.value = ((self.value << 8) | self.bits.get(8)) & 0xFFFFFFFF
            self.length = (self.length << 8) & 0xFFFFFFFF
        p = probs[index]
        x = p * (self.length >> 11)
        if self.value >= x:
            probs[index] = p - (p >> 5)
            self.value -= x
            self.length -= x
            return 1
        probs[index] = p + ((2048 - p) >> 5)
        self.length = x
        return 0


LIT_NEXT = [0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 4, 5]

NUM_STATES = 12
NUM_LIT_STATES = 7
MIN_MATCH_LEN = 2
MAX_MATCH_LEN = 257
NUM_HUGE_MATCH_CODES = 1
NUM_SECONDARY_LENGTHS = 249
LOWEST_USABLE_MATCH_SLOT = 1
HUGE_BASE = [MAX_MATCH_LEN + 1, MAX_MATCH_LEN + 1 + 256,
             MAX_MATCH_LEN + 1 + 256 + 1024, MAX_MATCH_LEN + 1 + 256 + 1024 + 4096]
HUGE_BITS = [8, 10, 12, 16]


def position_slots(dict_size_log2):
    extra_bits = [0] * 128
    j = 0
    for i in range(0, 128, 2):
        extra_bits[i] = extra_bits[i + 1] = j
        if i != 0 and j < 25:
            j += 1
    base = [0] * 128
    j = 0
    for i in range(128):
        base[i] = j
        j += 1 << extra_bits[i]

    num_slots_table = [30, 32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 58, 66]
    num_slots = num_slots_table[dict_size_log2 - 15]
    return extra_bits, base, num_slots


def decompress(data, dict_size_log2=20, table_update_rate=0, verify_adler=True):
    extra_bits, base, num_slots = position_slots(dict_size_log2)
    mask = (1 << dict_size_log2) - 1

    # The compressor derives the table update settings from m_table_update_rate (0 = default rate 8).
    settings = [(4, 32), (5, 33), (6, 34), (7, 35), (8, 36), (16, 48), (32, 72), (64, 64),
                (98, 80), (128, 96), (192, 112), (256, 128), (512, 160), (1024, 192), (2048, 256),
                (4096, 320), (8192, 384), (16384, 448), (32768, 512), (65536, 576)]
    rate = table_update_rate or 8
    max_interval, adapt_rate = settings[max(1, min(rate, 20)) - 1]

    def make_model(n):
        return Model(n, max_interval, adapt_rate)

    lit_model = make_model(256)
    delta_lit_model = make_model(256)
    main_model = make_model(2 + (num_slots - LOWEST_USABLE_MATCH_SLOT) * 8)
    rep_len_models = [make_model(NUM_HUGE_MATCH_CODES + (MAX_MATCH_LEN - MIN_MATCH_LEN + 1)) for _ in range(2)]
    large_len_models = [make_model(NUM_HUGE_MATCH_CODES + NUM_SECONDARY_LENGTHS) for _ in range(2)]
    dist_lsb_model = make_model(16)

    huff_models = [lit_model, delta_lit_model, main_model,
                   rep_len_models[0], rep_len_models[1],
                   large_len_models[0], large_len_models[1], dist_lsb_model]

    probs = {
        'is_match': [1024] * NUM_STATES,
        'is_rep': [1024] * NUM_STATES,
        'is_rep0': [1024] * NUM_STATES,
        'is_rep0_len1': [1024] * NUM_STATES,
        'is_rep1': [1024] * NUM_STATES,
        'is_rep2': [1024] * NUM_STATES,
    }

    def reset_arith():
        for v in probs.values():
            for i in range(NUM_STATES):
                v[i] = 1024

    def reset_update_rates():
        for m in huff_models:
            m.reset_update_rate()

    bits = Bits(data)
    arith = Arithmetic(bits)
    out = bytearray()

    def huge_match_len():
        k = 0
        while k < 3:
            if not bits.get(1):
                break
            k += 1
        return HUGE_BASE[k] + bits.get(HUGE_BITS[k])

    while True:
        block_type = bits.get(2)

        if block_type == 0:  # sync block
            flush_type = bits.get(2)
            if flush_type == 1:
                reset_update_rates()
            elif flush_type == 2:
                reset_update_rates()
                for m in huff_models:
                    m.reset()
                reset_arith()
            bits.align()
            if bits.get(16) != 0 or bits.get(16) != 0xFFFF:
                raise ValueError("bad sync block")

        elif block_type == 2:  # raw block
            length = bits.get(24)
            check = bits.get(8)
            if check != ((length & 255) ^ ((length >> 8) & 255) ^ ((length >> 16) & 255)):
                raise ValueError("bad raw block")
            length += 1
            bits.align()
            for _ in range(length):
                out.append(bits.get(8))

        elif block_type == 1:  # compressed block
            arith.start()
            match_hist0 = match_hist1 = match_hist2 = match_hist3 = 1
            state = 0
            flush_type = bits.get(2)
            if flush_type == 1:
                reset_update_rates()
            elif flush_type == 2:
                reset_update_rates()
                for m in huff_models:
                    m.reset()
                reset_arith()

            while True:
                if not arith.bit(probs['is_match'], state):
                    if state < NUM_LIT_STATES:
                        sym = lit_model.decode(bits)
                    else:
                        src = len(out) - match_hist0
                        if src < 0:
                            raise ValueError("bad delta literal distance")
                        sym = delta_lit_model.decode(bits) ^ out[src]
                    out.append(sym & 0xFF)
                    state = LIT_NEXT[state]
                    continue

                match_len = 1
                if arith.bit(probs['is_rep'], state):
                    if arith.bit(probs['is_rep0'], state):
                        if arith.bit(probs['is_rep0_len1'], state):
                            state = 9 if state < NUM_LIT_STATES else 11
                        else:
                            match_len = rep_len_models[state >= NUM_LIT_STATES].decode(bits) + MIN_MATCH_LEN
                            if match_len == MAX_MATCH_LEN + 1:
                                match_len = huge_match_len()
                            state = 8 if state < NUM_LIT_STATES else 11
                    else:
                        match_len = rep_len_models[state >= NUM_LIT_STATES].decode(bits) + MIN_MATCH_LEN
                        if match_len == MAX_MATCH_LEN + 1:
                            match_len = huge_match_len()
                        if arith.bit(probs['is_rep1'], state):
                            match_hist0, match_hist1 = match_hist1, match_hist0
                        elif arith.bit(probs['is_rep2'], state):
                            match_hist0, match_hist1, match_hist2 = match_hist2, match_hist0, match_hist1
                        else:
                            match_hist0, match_hist1, match_hist2, match_hist3 = \
                                match_hist3, match_hist0, match_hist1, match_hist2
                        state = 8 if state < NUM_LIT_STATES else 11
                else:
                    sym = main_model.decode(bits) - 2
                    if sym < 0:
                        if sym == -2:  # end of block
                            break
                        # partial state reset
                        match_hist0 = match_hist1 = match_hist2 = match_hist3 = 1
                        state = 0
                        continue

                    match_len = (sym & 7) + MIN_MATCH_LEN
                    slot = (sym >> 3) + LOWEST_USABLE_MATCH_SLOT

                    if match_len == 9:
                        match_len += large_len_models[state >= NUM_LIT_STATES].decode(bits)
                        if match_len == MAX_MATCH_LEN + 1:
                            match_len = huge_match_len()

                    nb = extra_bits[slot]
                    if nb < 3:
                        extra = bits.get(nb)
                    else:
                        extra = 0
                        if nb > 4:
                            extra = bits.get(nb - 4) << 4
                        extra += dist_lsb_model.decode(bits)

                    match_hist3 = match_hist2
                    match_hist2 = match_hist1
                    match_hist1 = match_hist0
                    match_hist0 = base[slot] + extra
                    state = NUM_LIT_STATES if state < NUM_LIT_STATES else NUM_LIT_STATES + 3

                if match_hist0 > len(out):
                    raise ValueError("bad match distance")
                src = len(out) - match_hist0
                for i in range(match_len):
                    out.append(out[src + i])

            bits.align()

        elif block_type == 3:  # EOF block
            break

        else:
            raise ValueError("bad block type")

    bits.align()
    file_adler = (bits.get(16) << 16) | bits.get(16)
    if verify_adler:
        if adler32(bytes(out)) != file_adler:
            raise ValueError("adler32 mismatch: stream says 0x%08X" % file_adler)

    return bytes(out)


def adler32(data):
    MOD = 65521
    s1, s2 = 1, 0
    for i in range(0, len(data), 5552):
        block = data[i:i + 5552]
        for b in block:
            s1 += b
            s2 += s1
        s1 %= MOD
        s2 %= MOD
    return (s2 << 16) | s1


def main(argv):
    if len(argv) < 3:
        print("usage: %s compressed.lzham output.bin [--dict N] [--rate N]" % argv[0])
        return 2
    dict_size_log2 = 20
    rate = 0
    args = argv[3:]
    i = 0
    while i < len(args):
        if args[i] == "--dict":
            dict_size_log2 = int(args[i + 1])
            i += 2
        elif args[i] == "--rate":
            rate = int(args[i + 1])
            i += 2
        else:
            print("unknown argument: %s" % args[i])
            return 2
    with open(argv[1], "rb") as f:
        data = f.read()
    out = decompress(data, dict_size_log2, rate)
    with open(argv[2], "wb") as f:
        f.write(out)
    print("decoded %d bytes" % len(out))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
