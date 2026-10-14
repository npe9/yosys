# sva_fuzz report -- seed 11, 400 cases, bound 12

- yosys: `/Users/npe/src/sv-yosys/third_party/yosys/build/yosys`
- smtbmc: `/Users/npe/src/sv-yosys/third_party/yosys/build/yosys-smtbmc`, solver z3
- wall time: 4.5 s (0.01 s/case)

## Totals

- cases: 400
- agreement: 395 (quiet 98, cex 297)
- bug-class findings: 5
- class C: 5
- determinism guard: PASS (all 400 cases ran the sim leg twice with identical summaries)

## Class C signatures (frontend/lowering gaps, deduped)

- `sva2check: unsupported property operator '##' in $auto$` -- 5 case(s); first repro: `/Users/npe/src/sv-yosys/third_party/yosys/tests/sva_fuzz/failures/C-11-10` (seed 11 index 10)

## Full per-case log

| index | class | detail | wall s |
|---|---|---|---|
| 0 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 1 | agree-cex | cex at step 3 reproduced in sim | 0.10 |
| 2 | agree-cex | cex at step 4 reproduced in sim | 0.10 |
| 3 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 4 | agree-cex | cex at step 2 reproduced in sim | 0.10 |
| 5 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 6 | agree-quiet | both legs quiet to bound 12 | 0.12 |
| 7 | agree-cex | cex at step 8 reproduced in sim | 0.10 |
| 8 | agree-cex | cex at step 6 reproduced in sim | 0.10 |
| 9 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 10 | C | sim-leg error sig: sva2check: unsupported property operator '##' in $auto$ | 0.03 |
| 11 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 12 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 13 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 14 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 15 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 16 | agree-cex | cex at step 4 reproduced in sim | 0.10 |
| 17 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 18 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 19 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 20 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 21 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 22 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 23 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 24 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 25 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 26 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 27 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 28 | agree-cex | cex at step 11 reproduced in sim | 0.12 |
| 29 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 30 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 31 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 32 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 33 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 34 | agree-cex | cex at step 10 reproduced in sim | 0.10 |
| 35 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 36 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 37 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 38 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 39 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 40 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 41 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 42 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 43 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 44 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 45 | agree-cex | cex at step 5 reproduced in sim | 0.10 |
| 46 | agree-cex | cex at step 10 reproduced in sim | 0.11 |
| 47 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 48 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 49 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 50 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 51 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 52 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 53 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 54 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 55 | C | sim-leg error sig: sva2check: unsupported property operator '##' in $auto$ | 0.01 |
| 56 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 57 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 58 | agree-cex | cex at step 9 reproduced in sim | 0.10 |
| 59 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 60 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 61 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 62 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 63 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 64 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 65 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 66 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 67 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 68 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 69 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 70 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 71 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 72 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 73 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 74 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 75 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 76 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 77 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 78 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 79 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 80 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 81 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 82 | agree-cex | cex at step 7 reproduced in sim | 0.08 |
| 83 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 84 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 85 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 86 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 87 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 88 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 89 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 90 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 91 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 92 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 93 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 94 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 95 | agree-cex | cex at step 8 reproduced in sim | 0.10 |
| 96 | agree-cex | cex at step 6 reproduced in sim | 0.10 |
| 97 | agree-cex | cex at step 3 reproduced in sim | 0.11 |
| 98 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 99 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 100 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 101 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 102 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 103 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 104 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 105 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 106 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 107 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 108 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 109 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 110 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 111 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 112 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 113 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 114 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 115 | agree-cex | cex at step 9 reproduced in sim | 0.10 |
| 116 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 117 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 118 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 119 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 120 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 121 | agree-cex | cex at step 10 reproduced in sim | 0.09 |
| 122 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 123 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 124 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 125 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 126 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 127 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 128 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 129 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 130 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 131 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 132 | agree-cex | cex at step 11 reproduced in sim | 0.09 |
| 133 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 134 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 135 | agree-quiet | both legs quiet to bound 12 | 0.12 |
| 136 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 137 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 138 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 139 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 140 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 141 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 142 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 143 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 144 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 145 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 146 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 147 | C | sim-leg error sig: sva2check: unsupported property operator '##' in $auto$ | 0.01 |
| 148 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 149 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 150 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 151 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 152 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 153 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 154 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 155 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 156 | agree-cex | cex at step 5 reproduced in sim | 0.10 |
| 157 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 158 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 159 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 160 | agree-cex | cex at step 11 reproduced in sim | 0.09 |
| 161 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 162 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 163 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 164 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 165 | agree-cex | cex at step 6 reproduced in sim | 0.10 |
| 166 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 167 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 168 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 169 | agree-cex | cex at step 10 reproduced in sim | 0.09 |
| 170 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 171 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 172 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 173 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 174 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 175 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 176 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 177 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 178 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 179 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 180 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 181 | agree-cex | cex at step 11 reproduced in sim | 0.11 |
| 182 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 183 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 184 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 185 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 186 | agree-cex | cex at step 9 reproduced in sim | 0.11 |
| 187 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 188 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 189 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 190 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 191 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 192 | agree-cex | cex at step 8 reproduced in sim | 0.08 |
| 193 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 194 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 195 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 196 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 197 | agree-cex | cex at step 9 reproduced in sim | 0.10 |
| 198 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 199 | C | sim-leg error sig: sva2check: unsupported property operator '##' in $auto$ | 0.01 |
| 200 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 201 | agree-cex | cex at step 2 reproduced in sim | 0.10 |
| 202 | agree-cex | cex at step 7 reproduced in sim | 0.11 |
| 203 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 204 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 205 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 206 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 207 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 208 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 209 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 210 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 211 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 212 | agree-cex | cex at step 1 reproduced in sim | 0.10 |
| 213 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 214 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 215 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 216 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 217 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 218 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 219 | agree-cex | cex at step 6 reproduced in sim | 0.08 |
| 220 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 221 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 222 | agree-cex | cex at step 9 reproduced in sim | 0.11 |
| 223 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 224 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 225 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 226 | agree-cex | cex at step 5 reproduced in sim | 0.11 |
| 227 | agree-cex | cex at step 10 reproduced in sim | 0.09 |
| 228 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 229 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 230 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 231 | agree-cex | cex at step 9 reproduced in sim | 0.09 |
| 232 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 233 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 234 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 235 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 236 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 237 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 238 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 239 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 240 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 241 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 242 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 243 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 244 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 245 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 246 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 247 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 248 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 249 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 250 | agree-cex | cex at step 10 reproduced in sim | 0.10 |
| 251 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 252 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 253 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 254 | agree-cex | cex at step 6 reproduced in sim | 0.10 |
| 255 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 256 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 257 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 258 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 259 | agree-cex | cex at step 7 reproduced in sim | 0.10 |
| 260 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 261 | agree-cex | cex at step 9 reproduced in sim | 0.10 |
| 262 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 263 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 264 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 265 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 266 | agree-cex | cex at step 10 reproduced in sim | 0.10 |
| 267 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 268 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 269 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 270 | agree-cex | cex at step 10 reproduced in sim | 0.11 |
| 271 | agree-cex | cex at step 8 reproduced in sim | 0.10 |
| 272 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 273 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 274 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 275 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 276 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 277 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 278 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 279 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 280 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 281 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 282 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 283 | agree-cex | cex at step 10 reproduced in sim | 0.10 |
| 284 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 285 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 286 | agree-cex | cex at step 8 reproduced in sim | 0.08 |
| 287 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 288 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 289 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 290 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 291 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 292 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 293 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 294 | agree-cex | cex at step 11 reproduced in sim | 0.09 |
| 295 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 296 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 297 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 298 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 299 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 300 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 301 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 302 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 303 | agree-cex | cex at step 10 reproduced in sim | 0.09 |
| 304 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 305 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 306 | agree-cex | cex at step 9 reproduced in sim | 0.09 |
| 307 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 308 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 309 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 310 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 311 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 312 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 313 | agree-cex | cex at step 9 reproduced in sim | 0.10 |
| 314 | agree-cex | cex at step 4 reproduced in sim | 0.10 |
| 315 | agree-cex | cex at step 2 reproduced in sim | 0.10 |
| 316 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 317 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 318 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 319 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 320 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 321 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 322 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 323 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 324 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 325 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 326 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 327 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 328 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 329 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 330 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 331 | agree-cex | cex at step 6 reproduced in sim | 0.08 |
| 332 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 333 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 334 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 335 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 336 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 337 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 338 | agree-cex | cex at step 11 reproduced in sim | 0.09 |
| 339 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 340 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 341 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 342 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 343 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 344 | agree-cex | cex at step 1 reproduced in sim | 0.10 |
| 345 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 346 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 347 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 348 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 349 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 350 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 351 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 352 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 353 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 354 | agree-cex | cex at step 4 reproduced in sim | 0.10 |
| 355 | agree-cex | cex at step 7 reproduced in sim | 0.12 |
| 356 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 357 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 358 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 359 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 360 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 361 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 362 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 363 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 364 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 365 | agree-cex | cex at step 5 reproduced in sim | 0.10 |
| 366 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 367 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 368 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 369 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 370 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 371 | agree-cex | cex at step 7 reproduced in sim | 0.10 |
| 372 | agree-cex | cex at step 9 reproduced in sim | 0.10 |
| 373 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 374 | agree-cex | cex at step 7 reproduced in sim | 0.10 |
| 375 | agree-cex | cex at step 8 reproduced in sim | 0.10 |
| 376 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 377 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 378 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 379 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 380 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 381 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 382 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 383 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 384 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 385 | agree-cex | cex at step 2 reproduced in sim | 0.10 |
| 386 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 387 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 388 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 389 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 390 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 391 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 392 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 393 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 394 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 395 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 396 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 397 | agree-cex | cex at step 7 reproduced in sim | 0.08 |
| 398 | C | sim-leg error sig: sva2check: unsupported property operator '##' in $auto$ | 0.01 |
| 399 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
