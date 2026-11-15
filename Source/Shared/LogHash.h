// LogHash.h: passwords never go into log files in plain text. LogPw() turns a password into a
// short salted SHA-256 fingerprint ("#3f9a1c0b7e2d45a8"): the same password always gives the same
// fingerprint, so logs can still show whether two passwords matched, but the password itself
// can't be read from them.
//////////////////////////////////////////////////////////////////////

#pragma once

#include <cstring>
#include <string>

namespace LogHash {

inline unsigned int Rotr(unsigned int x, int n) { return (x >> n) | (x << (32 - n)); }

// SHA-256 of a byte string, as 64 hex characters.
inline std::string Sha256Hex(const std::string & msg)
{
	static const unsigned int k[64] = {
		0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
		0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
		0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
		0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
		0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
		0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
		0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
		0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2 };
	unsigned int h[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };

	std::string m = msg;
	const unsigned long long bits = (unsigned long long)msg.size() * 8ull;
	m += (char)0x80;
	while (m.size() % 64 != 56) m += (char)0;
	for (int i = 7; i >= 0; i--) m += (char)((bits >> (i * 8)) & 0xff);

	for (size_t off = 0; off < m.size(); off += 64) {
		unsigned int w[64];
		for (int i = 0; i < 16; i++)
			w[i] = ((unsigned int)(unsigned char)m[off + i*4] << 24) | ((unsigned int)(unsigned char)m[off + i*4 + 1] << 16)
				 | ((unsigned int)(unsigned char)m[off + i*4 + 2] << 8) | (unsigned int)(unsigned char)m[off + i*4 + 3];
		for (int i = 16; i < 64; i++) {
			const unsigned int s0 = Rotr(w[i-15], 7) ^ Rotr(w[i-15], 18) ^ (w[i-15] >> 3);
			const unsigned int s1 = Rotr(w[i-2], 17) ^ Rotr(w[i-2], 19) ^ (w[i-2] >> 10);
			w[i] = w[i-16] + s0 + w[i-7] + s1;
		}
		unsigned int a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
		for (int i = 0; i < 64; i++) {
			const unsigned int S1 = Rotr(e, 6) ^ Rotr(e, 11) ^ Rotr(e, 25);
			const unsigned int ch = (e & f) ^ (~e & g);
			const unsigned int t1 = hh + S1 + ch + k[i] + w[i];
			const unsigned int S0 = Rotr(a, 2) ^ Rotr(a, 13) ^ Rotr(a, 22);
			const unsigned int mj = (a & b) ^ (a & c) ^ (b & c);
			const unsigned int t2 = S0 + mj;
			hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
		}
		h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
	}
	static const char kHex[] = "0123456789abcdef";
	std::string out;
	for (int i = 0; i < 8; i++)
		for (int j = 28; j >= 0; j -= 4) out += kHex[(h[i] >> j) & 15];
	return out;
}

} // namespace LogHash

// The fingerprint written to logs in place of a password.
inline std::string LogPw(const char * pPassword)
{
	if (pPassword == NULL) return "#none";
	char cPw[64] = {};
	strncpy(cPw, pPassword, sizeof(cPw) - 1);
	return "#" + LogHash::Sha256Hex(std::string("HelbreathReimagined-log:") + cPw).substr(0, 16);
}
