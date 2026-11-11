#include "inflate.hpp"
#include <cstring>

namespace sf {

uint32_t crc32(const uint8_t* data, size_t n, uint32_t crc) {
    static uint32_t table[256];
    static bool init = false;
    if (!init) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
        init = true;
    }
    crc = ~crc;
    for (size_t i = 0; i < n; ++i) crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

namespace {
struct Huff {
    uint16_t count[16];
    uint16_t symbol[288];
};

struct Inflater {
    const uint8_t* in = nullptr;
    size_t n = 0, pos = 0;
    uint32_t bitbuf = 0;
    int bitcnt = 0;
    Bytes out;
    size_t max_out = 0;

    int bit() {
        if (bitcnt == 0) {
            if (pos >= n) throw Error("inflate: unexpected end of data");
            bitbuf = in[pos++];
            bitcnt = 8;
        }
        int b = bitbuf & 1;
        bitbuf >>= 1;
        --bitcnt;
        return b;
    }
    uint32_t bits(int need) {
        uint32_t v = 0;
        for (int i = 0; i < need; ++i) v |= static_cast<uint32_t>(bit()) << i;
        return v;
    }
    // Canonical Huffman decode, as in zlib's reference "puff".
    int decode(const Huff& h) {
        int code = 0, first = 0, index = 0;
        for (int len = 1; len <= 15; ++len) {
            code |= bit();
            int count = h.count[len];
            if (code - count < first) return h.symbol[index + (code - first)];
            index += count;
            first += count;
            first <<= 1;
            code <<= 1;
        }
        throw Error("inflate: invalid Huffman code");
    }
    static void build(Huff& h, const uint16_t* length, int n) {
        std::memset(h.count, 0, sizeof h.count);
        for (int i = 0; i < n; ++i) ++h.count[length[i]];
        uint16_t offs[16];
        offs[1] = 0;
        for (int len = 1; len < 15; ++len) offs[len + 1] = offs[len] + h.count[len];
        for (int i = 0; i < n; ++i) if (length[i]) h.symbol[offs[length[i]]++] = static_cast<uint16_t>(i);
    }

    void codes(const Huff& lit, const Huff& dist) {
        static const uint16_t lbase[] = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
        static const uint16_t lext[] = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
        static const uint16_t dbase[] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
        static const uint16_t dext[] = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
        while (true) {
            int sym = decode(lit);
            if (sym < 256) {
                if (out.size() >= max_out) throw Error("inflate: output too large");
                out.push_back(static_cast<uint8_t>(sym));
            } else if (sym == 256) return;
            else {
                sym -= 257;
                if (sym >= 29) throw Error("inflate: bad length symbol");
                size_t len = lbase[sym] + bits(lext[sym]);
                int ds = decode(dist);
                if (ds >= 30) throw Error("inflate: bad distance symbol");
                size_t d = dbase[ds] + bits(dext[ds]);
                if (d > out.size()) throw Error("inflate: distance too far back");
                if (out.size() + len > max_out) throw Error("inflate: output too large");
                size_t from = out.size() - d;
                for (size_t k = 0; k < len; ++k) out.push_back(out[from + k]);
            }
        }
    }

    void run() {
        int last;
        do {
            last = bit();
            int type = static_cast<int>(bits(2));
            if (type == 0) {
                bitbuf = 0; bitcnt = 0;
                if (pos + 4 > n) throw Error("inflate: truncated stored block");
                uint32_t len = in[pos] | (in[pos + 1] << 8), nlen = in[pos + 2] | (in[pos + 3] << 8);
                pos += 4;
                if ((len ^ 0xFFFF) != nlen) throw Error("inflate: stored block length mismatch");
                if (pos + len > n) throw Error("inflate: truncated stored block");
                if (out.size() + len > max_out) throw Error("inflate: output too large");
                out.insert(out.end(), in + pos, in + pos + len);
                pos += len;
            } else if (type == 1) {
                static Huff lit, dist;
                static bool ready = false;
                if (!ready) {
                    uint16_t l[288];
                    int i = 0;
                    for (; i < 144; ++i) l[i] = 8;
                    for (; i < 256; ++i) l[i] = 9;
                    for (; i < 280; ++i) l[i] = 7;
                    for (; i < 288; ++i) l[i] = 8;
                    build(lit, l, 288);
                    uint16_t d[30];
                    for (int k = 0; k < 30; ++k) d[k] = 5;
                    build(dist, d, 30);
                    ready = true;
                }
                codes(lit, dist);
            } else if (type == 2) {
                static const uint8_t order[19] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
                int nlen = static_cast<int>(bits(5)) + 257, ndist = static_cast<int>(bits(5)) + 1, ncode = static_cast<int>(bits(4)) + 4;
                if (nlen > 286 || ndist > 30) throw Error("inflate: bad counts");
                uint16_t lengths[320] = {0};
                for (int i = 0; i < ncode; ++i) lengths[order[i]] = static_cast<uint16_t>(bits(3));
                Huff lencode;
                build(lencode, lengths, 19);
                int idx = 0;
                uint16_t ll[320] = {0};
                while (idx < nlen + ndist) {
                    int sym = decode(lencode);
                    if (sym < 16) ll[idx++] = static_cast<uint16_t>(sym);
                    else {
                        uint16_t prev = 0;
                        int rep;
                        if (sym == 16) {
                            if (idx == 0) throw Error("inflate: repeat with no previous length");
                            prev = ll[idx - 1];
                            rep = 3 + static_cast<int>(bits(2));
                        } else if (sym == 17) rep = 3 + static_cast<int>(bits(3));
                        else rep = 11 + static_cast<int>(bits(7));
                        if (idx + rep > nlen + ndist) throw Error("inflate: too many lengths");
                        while (rep--) ll[idx++] = prev;
                    }
                }
                Huff lit, dist;
                build(lit, ll, nlen);
                build(dist, ll + nlen, ndist);
                codes(lit, dist);
            } else throw Error("inflate: invalid block type");
        } while (!last);
    }
};
} // namespace

Bytes inflate_raw(const uint8_t* data, size_t n, size_t size_hint, size_t max_out) {
    Inflater f;
    f.in = data; f.n = n; f.max_out = max_out;
    if (size_hint && size_hint <= max_out) f.out.reserve(size_hint);
    f.run();
    return std::move(f.out);
}

Bytes zlib_decompress(const uint8_t* data, size_t n, size_t size_hint, size_t max_out) {
    if (n < 6 || (data[0] & 0x0F) != 8 || ((data[0] << 8) | data[1]) % 31 != 0) throw Error("zlib: bad header");
    if (data[1] & 0x20) throw Error("zlib: preset dictionary unsupported");
    return inflate_raw(data + 2, n - 2, size_hint, max_out);
}

} // namespace sf
