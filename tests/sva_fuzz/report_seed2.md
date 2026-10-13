# sva_fuzz report -- seed 2, 600 cases, bound 12

- yosys: `/Users/npe/src/sv-yosys/third_party/yosys/build/yosys`
- smtbmc: `/Users/npe/src/sv-yosys/third_party/yosys/build/yosys-smtbmc`, solver z3
- wall time: 10.1 s (0.02 s/case)

## Totals

- cases: 600
- agreement: 586 (quiet 124, cex 462)
- bug-class findings: 14
- class C: 14
- determinism guard: PASS (all 600 cases ran the sim leg twice with identical summaries)

## Class C signatures (frontend/lowering gaps, deduped)

- `sva2check: unsupported sequence operator 'and' in $auto$` -- 14 case(s); first repro: `/Users/npe/src/sv-yosys/third_party/yosys/tests/sva_fuzz/failures/C-2-0` (seed 2 index 0)

## Full per-case log

| index | class | detail | wall s |
|---|---|---|---|
| 0 | C | sim-leg error sig: sva2check: unsupported sequence operator 'and' in $auto$ | 0.11 |
| 1 | agree-cex | cex at step 8 reproduced in sim | 0.16 |
| 2 | agree-cex | cex at step 10 reproduced in sim | 0.17 |
| 3 | agree-quiet | both legs quiet to bound 12 | 0.16 |
| 4 | agree-cex | cex at step 9 reproduced in sim | 0.15 |
| 5 | agree-cex | cex at step 0 reproduced in sim | 0.17 |
| 6 | agree-cex | cex at step 0 reproduced in sim | 0.14 |
| 7 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 8 | agree-cex | cex at step 0 reproduced in sim | 0.14 |
| 9 | agree-cex | cex at step 2 reproduced in sim | 0.13 |
| 10 | agree-quiet | both legs quiet to bound 12 | 0.16 |
| 11 | agree-cex | cex at step 6 reproduced in sim | 0.15 |
| 12 | agree-cex | cex at step 2 reproduced in sim | 0.13 |
| 13 | agree-cex | cex at step 0 reproduced in sim | 0.16 |
| 14 | agree-cex | cex at step 1 reproduced in sim | 0.14 |
| 15 | agree-cex | cex at step 3 reproduced in sim | 0.16 |
| 16 | agree-quiet | both legs quiet to bound 12 | 0.15 |
| 17 | agree-cex | cex at step 8 reproduced in sim | 0.14 |
| 18 | agree-cex | cex at step 0 reproduced in sim | 0.13 |
| 19 | C | sim-leg error sig: sva2check: unsupported sequence operator 'and' in $auto$ | 0.01 |
| 20 | agree-cex | cex at step 4 reproduced in sim | 0.11 |
| 21 | agree-cex | cex at step 0 reproduced in sim | 0.15 |
| 22 | agree-cex | cex at step 0 reproduced in sim | 0.13 |
| 23 | agree-cex | cex at step 3 reproduced in sim | 0.15 |
| 24 | agree-cex | cex at step 3 reproduced in sim | 0.13 |
| 25 | agree-cex | cex at step 8 reproduced in sim | 0.13 |
| 26 | agree-cex | cex at step 0 reproduced in sim | 0.11 |
| 27 | agree-cex | cex at step 10 reproduced in sim | 0.14 |
| 28 | agree-cex | cex at step 5 reproduced in sim | 0.13 |
| 29 | agree-cex | cex at step 3 reproduced in sim | 0.14 |
| 30 | agree-cex | cex at step 6 reproduced in sim | 0.12 |
| 31 | agree-cex | cex at step 2 reproduced in sim | 0.11 |
| 32 | agree-cex | cex at step 4 reproduced in sim | 0.12 |
| 33 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 34 | agree-cex | cex at step 0 reproduced in sim | 0.11 |
| 35 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 36 | agree-cex | cex at step 7 reproduced in sim | 0.12 |
| 37 | agree-cex | cex at step 1 reproduced in sim | 0.11 |
| 38 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 39 | agree-cex | cex at step 2 reproduced in sim | 0.11 |
| 40 | agree-cex | cex at step 4 reproduced in sim | 0.13 |
| 41 | agree-cex | cex at step 6 reproduced in sim | 0.12 |
| 42 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 43 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 44 | agree-cex | cex at step 5 reproduced in sim | 0.11 |
| 45 | agree-cex | cex at step 3 reproduced in sim | 0.12 |
| 46 | agree-cex | cex at step 11 reproduced in sim | 0.13 |
| 47 | agree-quiet | both legs quiet to bound 12 | 0.13 |
| 48 | agree-cex | cex at step 0 reproduced in sim | 0.12 |
| 49 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 50 | agree-cex | cex at step 0 reproduced in sim | 0.12 |
| 51 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 52 | C | sim-leg error sig: sva2check: unsupported sequence operator 'and' in $auto$ | 0.01 |
| 53 | agree-cex | cex at step 8 reproduced in sim | 0.13 |
| 54 | agree-cex | cex at step 3 reproduced in sim | 0.12 |
| 55 | agree-cex | cex at step 1 reproduced in sim | 0.13 |
| 56 | agree-cex | cex at step 4 reproduced in sim | 0.12 |
| 57 | agree-quiet | both legs quiet to bound 12 | 0.12 |
| 58 | agree-cex | cex at step 1 reproduced in sim | 0.10 |
| 59 | agree-cex | cex at step 1 reproduced in sim | 0.12 |
| 60 | agree-cex | cex at step 3 reproduced in sim | 0.13 |
| 61 | agree-cex | cex at step 7 reproduced in sim | 0.11 |
| 62 | agree-cex | cex at step 10 reproduced in sim | 0.11 |
| 63 | agree-cex | cex at step 1 reproduced in sim | 0.15 |
| 64 | agree-cex | cex at step 3 reproduced in sim | 0.12 |
| 65 | agree-quiet | both legs quiet to bound 12 | 0.12 |
| 66 | agree-cex | cex at step 1 reproduced in sim | 0.12 |
| 67 | agree-cex | cex at step 3 reproduced in sim | 0.10 |
| 68 | agree-quiet | both legs quiet to bound 12 | 0.13 |
| 69 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 70 | agree-cex | cex at step 0 reproduced in sim | 0.11 |
| 71 | agree-quiet | both legs quiet to bound 12 | 0.14 |
| 72 | agree-cex | cex at step 0 reproduced in sim | 0.11 |
| 73 | agree-quiet | both legs quiet to bound 12 | 0.16 |
| 74 | agree-cex | cex at step 4 reproduced in sim | 0.12 |
| 75 | agree-cex | cex at step 8 reproduced in sim | 0.12 |
| 76 | agree-cex | cex at step 2 reproduced in sim | 0.14 |
| 77 | agree-cex | cex at step 5 reproduced in sim | 0.12 |
| 78 | agree-cex | cex at step 0 reproduced in sim | 0.12 |
| 79 | agree-cex | cex at step 8 reproduced in sim | 0.14 |
| 80 | agree-cex | cex at step 6 reproduced in sim | 0.11 |
| 81 | agree-cex | cex at step 1 reproduced in sim | 0.10 |
| 82 | agree-quiet | both legs quiet to bound 12 | 0.17 |
| 83 | agree-quiet | both legs quiet to bound 12 | 0.16 |
| 84 | agree-cex | cex at step 4 reproduced in sim | 0.15 |
| 85 | agree-quiet | both legs quiet to bound 12 | 0.16 |
| 86 | agree-cex | cex at step 0 reproduced in sim | 0.13 |
| 87 | agree-quiet | both legs quiet to bound 12 | 0.12 |
| 88 | agree-cex | cex at step 0 reproduced in sim | 0.13 |
| 89 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 90 | agree-cex | cex at step 1 reproduced in sim | 0.12 |
| 91 | agree-cex | cex at step 0 reproduced in sim | 0.11 |
| 92 | agree-cex | cex at step 1 reproduced in sim | 0.13 |
| 93 | agree-quiet | both legs quiet to bound 12 | 0.13 |
| 94 | agree-quiet | both legs quiet to bound 12 | 0.17 |
| 95 | agree-quiet | both legs quiet to bound 12 | 0.14 |
| 96 | agree-cex | cex at step 11 reproduced in sim | 0.17 |
| 97 | agree-cex | cex at step 11 reproduced in sim | 0.14 |
| 98 | agree-cex | cex at step 3 reproduced in sim | 0.12 |
| 99 | agree-cex | cex at step 2 reproduced in sim | 0.12 |
| 100 | agree-cex | cex at step 1 reproduced in sim | 0.13 |
| 101 | agree-cex | cex at step 0 reproduced in sim | 0.12 |
| 102 | agree-cex | cex at step 5 reproduced in sim | 0.13 |
| 103 | agree-cex | cex at step 11 reproduced in sim | 0.14 |
| 104 | agree-cex | cex at step 6 reproduced in sim | 0.13 |
| 105 | agree-cex | cex at step 2 reproduced in sim | 0.12 |
| 106 | agree-cex | cex at step 0 reproduced in sim | 0.11 |
| 107 | agree-cex | cex at step 5 reproduced in sim | 0.13 |
| 108 | agree-cex | cex at step 8 reproduced in sim | 0.13 |
| 109 | agree-cex | cex at step 5 reproduced in sim | 0.14 |
| 110 | agree-cex | cex at step 3 reproduced in sim | 0.13 |
| 111 | agree-cex | cex at step 6 reproduced in sim | 0.11 |
| 112 | agree-cex | cex at step 0 reproduced in sim | 0.12 |
| 113 | agree-cex | cex at step 5 reproduced in sim | 0.13 |
| 114 | agree-cex | cex at step 9 reproduced in sim | 0.13 |
| 115 | agree-cex | cex at step 1 reproduced in sim | 0.14 |
| 116 | agree-cex | cex at step 2 reproduced in sim | 0.13 |
| 117 | agree-cex | cex at step 2 reproduced in sim | 0.13 |
| 118 | agree-cex | cex at step 2 reproduced in sim | 0.12 |
| 119 | agree-cex | cex at step 5 reproduced in sim | 0.13 |
| 120 | agree-cex | cex at step 5 reproduced in sim | 0.11 |
| 121 | agree-cex | cex at step 6 reproduced in sim | 0.13 |
| 122 | agree-cex | cex at step 0 reproduced in sim | 0.12 |
| 123 | agree-quiet | both legs quiet to bound 12 | 0.12 |
| 124 | agree-cex | cex at step 3 reproduced in sim | 0.11 |
| 125 | agree-cex | cex at step 5 reproduced in sim | 0.12 |
| 126 | agree-cex | cex at step 4 reproduced in sim | 0.12 |
| 127 | agree-quiet | both legs quiet to bound 12 | 0.14 |
| 128 | agree-cex | cex at step 7 reproduced in sim | 0.15 |
| 129 | agree-cex | cex at step 0 reproduced in sim | 0.13 |
| 130 | agree-quiet | both legs quiet to bound 12 | 0.14 |
| 131 | agree-cex | cex at step 10 reproduced in sim | 0.16 |
| 132 | agree-quiet | both legs quiet to bound 12 | 0.12 |
| 133 | agree-cex | cex at step 0 reproduced in sim | 0.15 |
| 134 | agree-cex | cex at step 1 reproduced in sim | 0.12 |
| 135 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 136 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 137 | agree-cex | cex at step 4 reproduced in sim | 0.13 |
| 138 | agree-cex | cex at step 11 reproduced in sim | 0.12 |
| 139 | agree-quiet | both legs quiet to bound 12 | 0.12 |
| 140 | agree-cex | cex at step 0 reproduced in sim | 0.12 |
| 141 | agree-cex | cex at step 2 reproduced in sim | 0.12 |
| 142 | C | sim-leg error sig: sva2check: unsupported sequence operator 'and' in $auto$ | 0.02 |
| 143 | agree-cex | cex at step 7 reproduced in sim | 0.13 |
| 144 | agree-cex | cex at step 0 reproduced in sim | 0.11 |
| 145 | agree-cex | cex at step 4 reproduced in sim | 0.12 |
| 146 | agree-cex | cex at step 3 reproduced in sim | 0.12 |
| 147 | agree-cex | cex at step 2 reproduced in sim | 0.13 |
| 148 | agree-cex | cex at step 6 reproduced in sim | 0.14 |
| 149 | C | sim-leg error sig: sva2check: unsupported sequence operator 'and' in $auto$ | 0.01 |
| 150 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 151 | agree-quiet | both legs quiet to bound 12 | 0.12 |
| 152 | agree-quiet | both legs quiet to bound 12 | 0.12 |
| 153 | agree-cex | cex at step 0 reproduced in sim | 0.13 |
| 154 | agree-cex | cex at step 0 reproduced in sim | 0.12 |
| 155 | agree-cex | cex at step 0 reproduced in sim | 0.13 |
| 156 | agree-cex | cex at step 11 reproduced in sim | 0.15 |
| 157 | agree-cex | cex at step 0 reproduced in sim | 0.11 |
| 158 | agree-cex | cex at step 7 reproduced in sim | 0.12 |
| 159 | agree-cex | cex at step 0 reproduced in sim | 0.13 |
| 160 | agree-cex | cex at step 1 reproduced in sim | 0.12 |
| 161 | agree-cex | cex at step 0 reproduced in sim | 0.13 |
| 162 | agree-cex | cex at step 2 reproduced in sim | 0.13 |
| 163 | agree-cex | cex at step 3 reproduced in sim | 0.13 |
| 164 | agree-cex | cex at step 2 reproduced in sim | 0.11 |
| 165 | C | sim-leg error sig: sva2check: unsupported sequence operator 'and' in $auto$ | 0.01 |
| 166 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 167 | agree-cex | cex at step 0 reproduced in sim | 0.12 |
| 168 | agree-cex | cex at step 3 reproduced in sim | 0.12 |
| 169 | agree-cex | cex at step 0 reproduced in sim | 0.12 |
| 170 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 171 | agree-cex | cex at step 10 reproduced in sim | 0.14 |
| 172 | agree-cex | cex at step 1 reproduced in sim | 0.12 |
| 173 | agree-cex | cex at step 7 reproduced in sim | 0.12 |
| 174 | agree-cex | cex at step 2 reproduced in sim | 0.11 |
| 175 | agree-quiet | both legs quiet to bound 12 | 0.13 |
| 176 | agree-cex | cex at step 6 reproduced in sim | 0.12 |
| 177 | agree-cex | cex at step 5 reproduced in sim | 0.13 |
| 178 | agree-cex | cex at step 3 reproduced in sim | 0.13 |
| 179 | agree-cex | cex at step 7 reproduced in sim | 0.12 |
| 180 | agree-cex | cex at step 10 reproduced in sim | 0.12 |
| 181 | agree-cex | cex at step 10 reproduced in sim | 0.16 |
| 182 | agree-cex | cex at step 9 reproduced in sim | 0.12 |
| 183 | agree-cex | cex at step 7 reproduced in sim | 0.12 |
| 184 | agree-cex | cex at step 0 reproduced in sim | 0.11 |
| 185 | agree-cex | cex at step 8 reproduced in sim | 0.14 |
| 186 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 187 | agree-cex | cex at step 11 reproduced in sim | 0.14 |
| 188 | C | sim-leg error sig: sva2check: unsupported sequence operator 'and' in $auto$ | 0.01 |
| 189 | agree-cex | cex at step 8 reproduced in sim | 0.12 |
| 190 | agree-cex | cex at step 10 reproduced in sim | 0.16 |
| 191 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 192 | agree-cex | cex at step 0 reproduced in sim | 0.13 |
| 193 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 194 | agree-cex | cex at step 10 reproduced in sim | 0.14 |
| 195 | agree-cex | cex at step 0 reproduced in sim | 0.12 |
| 196 | agree-cex | cex at step 0 reproduced in sim | 0.13 |
| 197 | agree-cex | cex at step 1 reproduced in sim | 0.13 |
| 198 | agree-cex | cex at step 7 reproduced in sim | 0.12 |
| 199 | agree-quiet | both legs quiet to bound 12 | 0.13 |
| 200 | agree-cex | cex at step 0 reproduced in sim | 0.14 |
| 201 | agree-quiet | both legs quiet to bound 12 | 0.17 |
| 202 | agree-quiet | both legs quiet to bound 12 | 0.13 |
| 203 | agree-quiet | both legs quiet to bound 12 | 0.14 |
| 204 | agree-cex | cex at step 8 reproduced in sim | 0.14 |
| 205 | agree-cex | cex at step 4 reproduced in sim | 0.11 |
| 206 | agree-cex | cex at step 0 reproduced in sim | 0.14 |
| 207 | agree-cex | cex at step 5 reproduced in sim | 0.14 |
| 208 | agree-cex | cex at step 0 reproduced in sim | 0.11 |
| 209 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 210 | agree-cex | cex at step 3 reproduced in sim | 0.12 |
| 211 | agree-cex | cex at step 4 reproduced in sim | 0.13 |
| 212 | agree-cex | cex at step 10 reproduced in sim | 0.15 |
| 213 | agree-cex | cex at step 4 reproduced in sim | 0.13 |
| 214 | agree-quiet | both legs quiet to bound 12 | 0.12 |
| 215 | agree-cex | cex at step 0 reproduced in sim | 0.17 |
| 216 | agree-cex | cex at step 3 reproduced in sim | 0.15 |
| 217 | agree-cex | cex at step 9 reproduced in sim | 0.14 |
| 218 | agree-cex | cex at step 5 reproduced in sim | 0.13 |
| 219 | agree-quiet | both legs quiet to bound 12 | 0.13 |
| 220 | agree-cex | cex at step 0 reproduced in sim | 0.11 |
| 221 | agree-cex | cex at step 5 reproduced in sim | 0.10 |
| 222 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 223 | agree-cex | cex at step 2 reproduced in sim | 0.12 |
| 224 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 225 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 226 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 227 | agree-quiet | both legs quiet to bound 12 | 0.12 |
| 228 | agree-cex | cex at step 0 reproduced in sim | 0.12 |
| 229 | agree-cex | cex at step 5 reproduced in sim | 0.13 |
| 230 | agree-cex | cex at step 7 reproduced in sim | 0.17 |
| 231 | C | sim-leg error sig: sva2check: unsupported sequence operator 'and' in $auto$ | 0.01 |
| 232 | agree-cex | cex at step 2 reproduced in sim | 0.16 |
| 233 | agree-cex | cex at step 7 reproduced in sim | 0.15 |
| 234 | agree-cex | cex at step 0 reproduced in sim | 0.15 |
| 235 | agree-cex | cex at step 1 reproduced in sim | 0.17 |
| 236 | agree-cex | cex at step 0 reproduced in sim | 0.13 |
| 237 | agree-cex | cex at step 6 reproduced in sim | 0.16 |
| 238 | agree-cex | cex at step 0 reproduced in sim | 0.17 |
| 239 | agree-cex | cex at step 2 reproduced in sim | 0.18 |
| 240 | agree-cex | cex at step 10 reproduced in sim | 0.18 |
| 241 | agree-cex | cex at step 1 reproduced in sim | 0.16 |
| 242 | agree-cex | cex at step 1 reproduced in sim | 0.14 |
| 243 | agree-quiet | both legs quiet to bound 12 | 0.16 |
| 244 | agree-cex | cex at step 3 reproduced in sim | 0.14 |
| 245 | agree-cex | cex at step 0 reproduced in sim | 0.12 |
| 246 | agree-cex | cex at step 3 reproduced in sim | 0.13 |
| 247 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 248 | agree-cex | cex at step 6 reproduced in sim | 0.13 |
| 249 | agree-quiet | both legs quiet to bound 12 | 0.14 |
| 250 | agree-cex | cex at step 4 reproduced in sim | 0.13 |
| 251 | agree-quiet | both legs quiet to bound 12 | 0.12 |
| 252 | agree-cex | cex at step 0 reproduced in sim | 0.14 |
| 253 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 254 | agree-quiet | both legs quiet to bound 12 | 0.17 |
| 255 | agree-cex | cex at step 10 reproduced in sim | 0.16 |
| 256 | agree-cex | cex at step 1 reproduced in sim | 0.12 |
| 257 | agree-cex | cex at step 2 reproduced in sim | 0.15 |
| 258 | agree-cex | cex at step 0 reproduced in sim | 0.13 |
| 259 | agree-cex | cex at step 7 reproduced in sim | 0.14 |
| 260 | agree-quiet | both legs quiet to bound 12 | 0.13 |
| 261 | agree-cex | cex at step 9 reproduced in sim | 0.13 |
| 262 | agree-quiet | both legs quiet to bound 12 | 0.13 |
| 263 | agree-cex | cex at step 3 reproduced in sim | 0.16 |
| 264 | agree-quiet | both legs quiet to bound 12 | 0.13 |
| 265 | agree-cex | cex at step 4 reproduced in sim | 0.15 |
| 266 | agree-cex | cex at step 0 reproduced in sim | 0.17 |
| 267 | agree-cex | cex at step 8 reproduced in sim | 0.16 |
| 268 | agree-cex | cex at step 0 reproduced in sim | 0.15 |
| 269 | agree-cex | cex at step 6 reproduced in sim | 0.16 |
| 270 | agree-cex | cex at step 0 reproduced in sim | 0.14 |
| 271 | agree-quiet | both legs quiet to bound 12 | 0.14 |
| 272 | agree-quiet | both legs quiet to bound 12 | 0.15 |
| 273 | agree-cex | cex at step 6 reproduced in sim | 0.15 |
| 274 | agree-cex | cex at step 6 reproduced in sim | 0.16 |
| 275 | agree-cex | cex at step 6 reproduced in sim | 0.13 |
| 276 | agree-cex | cex at step 6 reproduced in sim | 0.15 |
| 277 | agree-cex | cex at step 4 reproduced in sim | 0.14 |
| 278 | agree-cex | cex at step 3 reproduced in sim | 0.13 |
| 279 | agree-quiet | both legs quiet to bound 12 | 0.12 |
| 280 | agree-cex | cex at step 7 reproduced in sim | 0.14 |
| 281 | agree-cex | cex at step 8 reproduced in sim | 0.17 |
| 282 | agree-quiet | both legs quiet to bound 12 | 0.12 |
| 283 | agree-cex | cex at step 1 reproduced in sim | 0.14 |
| 284 | agree-cex | cex at step 7 reproduced in sim | 0.14 |
| 285 | agree-cex | cex at step 0 reproduced in sim | 0.16 |
| 286 | agree-cex | cex at step 5 reproduced in sim | 0.17 |
| 287 | agree-cex | cex at step 10 reproduced in sim | 0.17 |
| 288 | agree-quiet | both legs quiet to bound 12 | 0.13 |
| 289 | agree-quiet | both legs quiet to bound 12 | 0.12 |
| 290 | agree-cex | cex at step 6 reproduced in sim | 0.16 |
| 291 | agree-cex | cex at step 6 reproduced in sim | 0.13 |
| 292 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 293 | agree-cex | cex at step 2 reproduced in sim | 0.12 |
| 294 | agree-cex | cex at step 0 reproduced in sim | 0.11 |
| 295 | agree-cex | cex at step 4 reproduced in sim | 0.11 |
| 296 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 297 | agree-cex | cex at step 2 reproduced in sim | 0.11 |
| 298 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 299 | agree-cex | cex at step 1 reproduced in sim | 0.13 |
| 300 | C | sim-leg error sig: sva2check: unsupported sequence operator 'and' in $auto$ | 0.02 |
| 301 | agree-cex | cex at step 4 reproduced in sim | 0.15 |
| 302 | agree-cex | cex at step 0 reproduced in sim | 0.14 |
| 303 | agree-cex | cex at step 9 reproduced in sim | 0.15 |
| 304 | C | sim-leg error sig: sva2check: unsupported sequence operator 'and' in $auto$ | 0.02 |
| 305 | agree-cex | cex at step 3 reproduced in sim | 0.14 |
| 306 | agree-cex | cex at step 0 reproduced in sim | 0.14 |
| 307 | agree-cex | cex at step 2 reproduced in sim | 0.14 |
| 308 | agree-cex | cex at step 4 reproduced in sim | 0.13 |
| 309 | agree-cex | cex at step 5 reproduced in sim | 0.11 |
| 310 | agree-cex | cex at step 3 reproduced in sim | 0.11 |
| 311 | agree-cex | cex at step 0 reproduced in sim | 0.14 |
| 312 | agree-quiet | both legs quiet to bound 12 | 0.13 |
| 313 | agree-quiet | both legs quiet to bound 12 | 0.15 |
| 314 | agree-cex | cex at step 7 reproduced in sim | 0.15 |
| 315 | agree-cex | cex at step 8 reproduced in sim | 0.13 |
| 316 | agree-quiet | both legs quiet to bound 12 | 0.14 |
| 317 | agree-cex | cex at step 3 reproduced in sim | 0.11 |
| 318 | agree-quiet | both legs quiet to bound 12 | 0.12 |
| 319 | agree-cex | cex at step 10 reproduced in sim | 0.13 |
| 320 | agree-quiet | both legs quiet to bound 12 | 0.15 |
| 321 | agree-cex | cex at step 0 reproduced in sim | 0.13 |
| 322 | agree-cex | cex at step 0 reproduced in sim | 0.11 |
| 323 | agree-cex | cex at step 0 reproduced in sim | 0.13 |
| 324 | agree-cex | cex at step 0 reproduced in sim | 0.12 |
| 325 | agree-cex | cex at step 7 reproduced in sim | 0.11 |
| 326 | agree-cex | cex at step 8 reproduced in sim | 0.12 |
| 327 | agree-cex | cex at step 6 reproduced in sim | 0.11 |
| 328 | agree-cex | cex at step 3 reproduced in sim | 0.13 |
| 329 | agree-cex | cex at step 0 reproduced in sim | 0.11 |
| 330 | agree-cex | cex at step 1 reproduced in sim | 0.14 |
| 331 | agree-cex | cex at step 3 reproduced in sim | 0.11 |
| 332 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 333 | agree-cex | cex at step 0 reproduced in sim | 0.11 |
| 334 | agree-cex | cex at step 0 reproduced in sim | 0.11 |
| 335 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 336 | agree-cex | cex at step 6 reproduced in sim | 0.15 |
| 337 | agree-cex | cex at step 0 reproduced in sim | 0.11 |
| 338 | agree-cex | cex at step 8 reproduced in sim | 0.13 |
| 339 | agree-cex | cex at step 5 reproduced in sim | 0.16 |
| 340 | agree-quiet | both legs quiet to bound 12 | 0.13 |
| 341 | C | sim-leg error sig: sva2check: unsupported sequence operator 'and' in $auto$ | 0.03 |
| 342 | agree-cex | cex at step 3 reproduced in sim | 0.13 |
| 343 | agree-quiet | both legs quiet to bound 12 | 0.12 |
| 344 | agree-cex | cex at step 11 reproduced in sim | 0.15 |
| 345 | agree-quiet | both legs quiet to bound 12 | 0.17 |
| 346 | agree-cex | cex at step 10 reproduced in sim | 0.13 |
| 347 | agree-cex | cex at step 0 reproduced in sim | 0.14 |
| 348 | agree-cex | cex at step 9 reproduced in sim | 0.12 |
| 349 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 350 | agree-cex | cex at step 2 reproduced in sim | 0.13 |
| 351 | agree-cex | cex at step 9 reproduced in sim | 0.13 |
| 352 | agree-cex | cex at step 5 reproduced in sim | 0.12 |
| 353 | agree-cex | cex at step 0 reproduced in sim | 0.13 |
| 354 | agree-cex | cex at step 7 reproduced in sim | 0.14 |
| 355 | agree-cex | cex at step 0 reproduced in sim | 0.11 |
| 356 | agree-cex | cex at step 3 reproduced in sim | 0.14 |
| 357 | agree-cex | cex at step 2 reproduced in sim | 0.13 |
| 358 | agree-cex | cex at step 0 reproduced in sim | 0.12 |
| 359 | agree-quiet | both legs quiet to bound 12 | 0.12 |
| 360 | agree-cex | cex at step 0 reproduced in sim | 0.13 |
| 361 | agree-cex | cex at step 0 reproduced in sim | 0.12 |
| 362 | agree-cex | cex at step 0 reproduced in sim | 0.11 |
| 363 | agree-quiet | both legs quiet to bound 12 | 0.15 |
| 364 | agree-cex | cex at step 0 reproduced in sim | 0.13 |
| 365 | agree-cex | cex at step 0 reproduced in sim | 0.13 |
| 366 | agree-cex | cex at step 11 reproduced in sim | 0.12 |
| 367 | agree-cex | cex at step 3 reproduced in sim | 0.12 |
| 368 | agree-cex | cex at step 7 reproduced in sim | 0.14 |
| 369 | agree-cex | cex at step 7 reproduced in sim | 0.17 |
| 370 | agree-cex | cex at step 4 reproduced in sim | 0.15 |
| 371 | agree-cex | cex at step 0 reproduced in sim | 0.14 |
| 372 | agree-cex | cex at step 0 reproduced in sim | 0.11 |
| 373 | agree-cex | cex at step 6 reproduced in sim | 0.11 |
| 374 | agree-cex | cex at step 1 reproduced in sim | 0.13 |
| 375 | agree-cex | cex at step 3 reproduced in sim | 0.14 |
| 376 | agree-cex | cex at step 3 reproduced in sim | 0.11 |
| 377 | agree-cex | cex at step 6 reproduced in sim | 0.12 |
| 378 | agree-quiet | both legs quiet to bound 12 | 0.12 |
| 379 | agree-cex | cex at step 1 reproduced in sim | 0.14 |
| 380 | agree-cex | cex at step 0 reproduced in sim | 0.11 |
| 381 | agree-quiet | both legs quiet to bound 12 | 0.16 |
| 382 | agree-cex | cex at step 3 reproduced in sim | 0.12 |
| 383 | agree-cex | cex at step 3 reproduced in sim | 0.12 |
| 384 | agree-cex | cex at step 1 reproduced in sim | 0.10 |
| 385 | agree-cex | cex at step 1 reproduced in sim | 0.10 |
| 386 | agree-cex | cex at step 0 reproduced in sim | 0.13 |
| 387 | agree-cex | cex at step 4 reproduced in sim | 0.11 |
| 388 | agree-cex | cex at step 6 reproduced in sim | 0.12 |
| 389 | agree-quiet | both legs quiet to bound 12 | 0.14 |
| 390 | agree-cex | cex at step 0 reproduced in sim | 0.14 |
| 391 | agree-quiet | both legs quiet to bound 12 | 0.13 |
| 392 | agree-cex | cex at step 2 reproduced in sim | 0.17 |
| 393 | agree-cex | cex at step 0 reproduced in sim | 0.13 |
| 394 | agree-cex | cex at step 0 reproduced in sim | 0.13 |
| 395 | agree-quiet | both legs quiet to bound 12 | 0.13 |
| 396 | agree-cex | cex at step 0 reproduced in sim | 0.16 |
| 397 | agree-cex | cex at step 10 reproduced in sim | 0.17 |
| 398 | agree-cex | cex at step 8 reproduced in sim | 0.16 |
| 399 | agree-cex | cex at step 7 reproduced in sim | 0.17 |
| 400 | agree-cex | cex at step 1 reproduced in sim | 0.15 |
| 401 | agree-cex | cex at step 7 reproduced in sim | 0.14 |
| 402 | agree-cex | cex at step 7 reproduced in sim | 0.12 |
| 403 | agree-cex | cex at step 0 reproduced in sim | 0.13 |
| 404 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 405 | agree-cex | cex at step 3 reproduced in sim | 0.12 |
| 406 | agree-cex | cex at step 1 reproduced in sim | 0.13 |
| 407 | agree-cex | cex at step 2 reproduced in sim | 0.13 |
| 408 | agree-quiet | both legs quiet to bound 12 | 0.13 |
| 409 | agree-cex | cex at step 7 reproduced in sim | 0.16 |
| 410 | agree-cex | cex at step 6 reproduced in sim | 0.14 |
| 411 | agree-quiet | both legs quiet to bound 12 | 0.16 |
| 412 | agree-cex | cex at step 0 reproduced in sim | 0.12 |
| 413 | agree-cex | cex at step 3 reproduced in sim | 0.11 |
| 414 | agree-cex | cex at step 4 reproduced in sim | 0.11 |
| 415 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 416 | agree-cex | cex at step 0 reproduced in sim | 0.14 |
| 417 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 418 | agree-cex | cex at step 0 reproduced in sim | 0.13 |
| 419 | agree-cex | cex at step 11 reproduced in sim | 0.19 |
| 420 | agree-cex | cex at step 1 reproduced in sim | 0.13 |
| 421 | agree-cex | cex at step 0 reproduced in sim | 0.12 |
| 422 | agree-cex | cex at step 0 reproduced in sim | 0.11 |
| 423 | agree-cex | cex at step 11 reproduced in sim | 0.14 |
| 424 | agree-cex | cex at step 4 reproduced in sim | 0.15 |
| 425 | C | sim-leg error sig: sva2check: unsupported sequence operator 'and' in $auto$ | 0.01 |
| 426 | agree-quiet | both legs quiet to bound 12 | 0.13 |
| 427 | agree-cex | cex at step 0 reproduced in sim | 0.12 |
| 428 | agree-cex | cex at step 6 reproduced in sim | 0.17 |
| 429 | agree-cex | cex at step 1 reproduced in sim | 0.12 |
| 430 | agree-cex | cex at step 3 reproduced in sim | 0.13 |
| 431 | agree-cex | cex at step 3 reproduced in sim | 0.15 |
| 432 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 433 | agree-cex | cex at step 5 reproduced in sim | 0.14 |
| 434 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 435 | agree-cex | cex at step 7 reproduced in sim | 0.14 |
| 436 | agree-cex | cex at step 8 reproduced in sim | 0.13 |
| 437 | agree-quiet | both legs quiet to bound 12 | 0.12 |
| 438 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 439 | agree-cex | cex at step 6 reproduced in sim | 0.12 |
| 440 | agree-cex | cex at step 11 reproduced in sim | 0.14 |
| 441 | agree-cex | cex at step 0 reproduced in sim | 0.11 |
| 442 | agree-cex | cex at step 9 reproduced in sim | 0.16 |
| 443 | agree-quiet | both legs quiet to bound 12 | 0.13 |
| 444 | agree-cex | cex at step 7 reproduced in sim | 0.15 |
| 445 | agree-cex | cex at step 7 reproduced in sim | 0.17 |
| 446 | agree-cex | cex at step 8 reproduced in sim | 0.17 |
| 447 | agree-cex | cex at step 0 reproduced in sim | 0.11 |
| 448 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 449 | agree-quiet | both legs quiet to bound 12 | 0.13 |
| 450 | agree-cex | cex at step 11 reproduced in sim | 0.28 |
| 451 | agree-cex | cex at step 4 reproduced in sim | 0.12 |
| 452 | agree-cex | cex at step 0 reproduced in sim | 0.14 |
| 453 | agree-quiet | both legs quiet to bound 12 | 0.12 |
| 454 | agree-cex | cex at step 0 reproduced in sim | 0.13 |
| 455 | agree-cex | cex at step 9 reproduced in sim | 0.17 |
| 456 | agree-cex | cex at step 11 reproduced in sim | 0.15 |
| 457 | agree-cex | cex at step 11 reproduced in sim | 0.13 |
| 458 | agree-cex | cex at step 5 reproduced in sim | 0.12 |
| 459 | agree-quiet | both legs quiet to bound 12 | 0.14 |
| 460 | agree-cex | cex at step 7 reproduced in sim | 0.14 |
| 461 | agree-quiet | both legs quiet to bound 12 | 0.12 |
| 462 | agree-cex | cex at step 0 reproduced in sim | 0.14 |
| 463 | agree-cex | cex at step 5 reproduced in sim | 0.13 |
| 464 | agree-cex | cex at step 0 reproduced in sim | 0.12 |
| 465 | agree-cex | cex at step 2 reproduced in sim | 0.13 |
| 466 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 467 | agree-cex | cex at step 7 reproduced in sim | 0.16 |
| 468 | agree-cex | cex at step 2 reproduced in sim | 0.14 |
| 469 | agree-cex | cex at step 2 reproduced in sim | 0.17 |
| 470 | agree-cex | cex at step 10 reproduced in sim | 0.15 |
| 471 | agree-cex | cex at step 11 reproduced in sim | 0.15 |
| 472 | agree-cex | cex at step 0 reproduced in sim | 0.14 |
| 473 | agree-cex | cex at step 3 reproduced in sim | 0.17 |
| 474 | agree-cex | cex at step 0 reproduced in sim | 0.14 |
| 475 | agree-cex | cex at step 0 reproduced in sim | 0.12 |
| 476 | agree-cex | cex at step 4 reproduced in sim | 0.14 |
| 477 | agree-cex | cex at step 2 reproduced in sim | 0.14 |
| 478 | agree-cex | cex at step 5 reproduced in sim | 0.15 |
| 479 | agree-cex | cex at step 7 reproduced in sim | 0.17 |
| 480 | agree-quiet | both legs quiet to bound 12 | 0.13 |
| 481 | agree-cex | cex at step 2 reproduced in sim | 0.13 |
| 482 | agree-quiet | both legs quiet to bound 12 | 0.12 |
| 483 | agree-quiet | both legs quiet to bound 12 | 0.12 |
| 484 | agree-cex | cex at step 7 reproduced in sim | 0.13 |
| 485 | agree-cex | cex at step 2 reproduced in sim | 0.11 |
| 486 | agree-cex | cex at step 0 reproduced in sim | 0.11 |
| 487 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 488 | agree-cex | cex at step 4 reproduced in sim | 0.12 |
| 489 | agree-quiet | both legs quiet to bound 12 | 0.13 |
| 490 | agree-cex | cex at step 7 reproduced in sim | 0.12 |
| 491 | agree-cex | cex at step 11 reproduced in sim | 0.14 |
| 492 | agree-cex | cex at step 11 reproduced in sim | 0.14 |
| 493 | agree-cex | cex at step 1 reproduced in sim | 0.12 |
| 494 | agree-cex | cex at step 3 reproduced in sim | 0.14 |
| 495 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 496 | agree-cex | cex at step 0 reproduced in sim | 0.11 |
| 497 | agree-cex | cex at step 4 reproduced in sim | 0.11 |
| 498 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 499 | agree-cex | cex at step 0 reproduced in sim | 0.13 |
| 500 | agree-cex | cex at step 0 reproduced in sim | 0.11 |
| 501 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 502 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 503 | agree-cex | cex at step 0 reproduced in sim | 0.11 |
| 504 | agree-quiet | both legs quiet to bound 12 | 0.13 |
| 505 | agree-cex | cex at step 2 reproduced in sim | 0.10 |
| 506 | agree-cex | cex at step 8 reproduced in sim | 0.12 |
| 507 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 508 | agree-cex | cex at step 0 reproduced in sim | 0.12 |
| 509 | agree-cex | cex at step 0 reproduced in sim | 0.12 |
| 510 | agree-cex | cex at step 1 reproduced in sim | 0.11 |
| 511 | agree-cex | cex at step 7 reproduced in sim | 0.13 |
| 512 | agree-cex | cex at step 4 reproduced in sim | 0.11 |
| 513 | agree-cex | cex at step 0 reproduced in sim | 0.11 |
| 514 | agree-cex | cex at step 4 reproduced in sim | 0.12 |
| 515 | agree-cex | cex at step 0 reproduced in sim | 0.13 |
| 516 | agree-cex | cex at step 10 reproduced in sim | 0.15 |
| 517 | agree-cex | cex at step 1 reproduced in sim | 0.12 |
| 518 | agree-cex | cex at step 0 reproduced in sim | 0.13 |
| 519 | agree-cex | cex at step 4 reproduced in sim | 0.15 |
| 520 | agree-cex | cex at step 6 reproduced in sim | 0.15 |
| 521 | agree-cex | cex at step 0 reproduced in sim | 0.11 |
| 522 | agree-cex | cex at step 8 reproduced in sim | 0.14 |
| 523 | agree-cex | cex at step 6 reproduced in sim | 0.16 |
| 524 | agree-cex | cex at step 6 reproduced in sim | 0.15 |
| 525 | agree-quiet | both legs quiet to bound 12 | 0.14 |
| 526 | agree-cex | cex at step 11 reproduced in sim | 0.15 |
| 527 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 528 | agree-cex | cex at step 9 reproduced in sim | 0.12 |
| 529 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 530 | agree-cex | cex at step 6 reproduced in sim | 0.14 |
| 531 | agree-cex | cex at step 0 reproduced in sim | 0.13 |
| 532 | agree-cex | cex at step 4 reproduced in sim | 0.11 |
| 533 | agree-cex | cex at step 4 reproduced in sim | 0.15 |
| 534 | agree-cex | cex at step 5 reproduced in sim | 0.18 |
| 535 | agree-cex | cex at step 2 reproduced in sim | 0.13 |
| 536 | agree-cex | cex at step 0 reproduced in sim | 0.15 |
| 537 | agree-cex | cex at step 6 reproduced in sim | 0.14 |
| 538 | agree-cex | cex at step 4 reproduced in sim | 0.12 |
| 539 | agree-cex | cex at step 6 reproduced in sim | 0.13 |
| 540 | agree-cex | cex at step 5 reproduced in sim | 0.14 |
| 541 | agree-cex | cex at step 6 reproduced in sim | 0.14 |
| 542 | agree-cex | cex at step 1 reproduced in sim | 0.16 |
| 543 | agree-cex | cex at step 1 reproduced in sim | 0.13 |
| 544 | agree-cex | cex at step 1 reproduced in sim | 0.13 |
| 545 | agree-cex | cex at step 1 reproduced in sim | 0.16 |
| 546 | agree-cex | cex at step 1 reproduced in sim | 0.13 |
| 547 | agree-cex | cex at step 11 reproduced in sim | 0.18 |
| 548 | agree-cex | cex at step 10 reproduced in sim | 0.17 |
| 549 | agree-cex | cex at step 11 reproduced in sim | 0.17 |
| 550 | agree-cex | cex at step 8 reproduced in sim | 0.13 |
| 551 | agree-cex | cex at step 4 reproduced in sim | 0.14 |
| 552 | agree-quiet | both legs quiet to bound 12 | 0.13 |
| 553 | agree-quiet | both legs quiet to bound 12 | 0.12 |
| 554 | agree-cex | cex at step 3 reproduced in sim | 0.11 |
| 555 | agree-cex | cex at step 3 reproduced in sim | 0.13 |
| 556 | agree-cex | cex at step 4 reproduced in sim | 0.10 |
| 557 | agree-cex | cex at step 3 reproduced in sim | 0.13 |
| 558 | agree-cex | cex at step 1 reproduced in sim | 0.13 |
| 559 | agree-quiet | both legs quiet to bound 12 | 0.12 |
| 560 | agree-cex | cex at step 2 reproduced in sim | 0.12 |
| 561 | agree-quiet | both legs quiet to bound 12 | 0.16 |
| 562 | agree-cex | cex at step 4 reproduced in sim | 0.13 |
| 563 | C | sim-leg error sig: sva2check: unsupported sequence operator 'and' in $auto$ | 0.01 |
| 564 | agree-cex | cex at step 5 reproduced in sim | 0.13 |
| 565 | agree-cex | cex at step 1 reproduced in sim | 0.11 |
| 566 | agree-cex | cex at step 0 reproduced in sim | 0.13 |
| 567 | agree-quiet | both legs quiet to bound 12 | 0.16 |
| 568 | agree-cex | cex at step 0 reproduced in sim | 0.16 |
| 569 | agree-cex | cex at step 9 reproduced in sim | 0.15 |
| 570 | agree-quiet | both legs quiet to bound 12 | 0.12 |
| 571 | agree-cex | cex at step 4 reproduced in sim | 0.13 |
| 572 | agree-cex | cex at step 3 reproduced in sim | 0.12 |
| 573 | agree-cex | cex at step 1 reproduced in sim | 0.12 |
| 574 | agree-cex | cex at step 0 reproduced in sim | 0.12 |
| 575 | agree-cex | cex at step 0 reproduced in sim | 0.11 |
| 576 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 577 | agree-cex | cex at step 0 reproduced in sim | 0.11 |
| 578 | agree-cex | cex at step 8 reproduced in sim | 0.15 |
| 579 | agree-cex | cex at step 2 reproduced in sim | 0.14 |
| 580 | agree-cex | cex at step 4 reproduced in sim | 0.15 |
| 581 | agree-quiet | both legs quiet to bound 12 | 0.13 |
| 582 | agree-quiet | both legs quiet to bound 12 | 0.12 |
| 583 | C | sim-leg error sig: sva2check: unsupported sequence operator 'and' in $auto$ | 0.02 |
| 584 | agree-cex | cex at step 7 reproduced in sim | 0.14 |
| 585 | agree-quiet | both legs quiet to bound 12 | 0.13 |
| 586 | agree-cex | cex at step 4 reproduced in sim | 0.14 |
| 587 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 588 | agree-cex | cex at step 7 reproduced in sim | 0.15 |
| 589 | agree-cex | cex at step 2 reproduced in sim | 0.11 |
| 590 | agree-cex | cex at step 0 reproduced in sim | 0.13 |
| 591 | agree-cex | cex at step 0 reproduced in sim | 0.12 |
| 592 | agree-quiet | both legs quiet to bound 12 | 0.12 |
| 593 | agree-quiet | both legs quiet to bound 12 | 0.14 |
| 594 | agree-cex | cex at step 7 reproduced in sim | 0.15 |
| 595 | agree-cex | cex at step 5 reproduced in sim | 0.14 |
| 596 | agree-cex | cex at step 1 reproduced in sim | 0.12 |
| 597 | agree-cex | cex at step 10 reproduced in sim | 0.12 |
| 598 | agree-cex | cex at step 5 reproduced in sim | 0.11 |
| 599 | agree-quiet | both legs quiet to bound 12 | 0.10 |
