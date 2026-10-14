# sva_fuzz report -- seed 12, 600 cases, bound 12

- yosys: `/Users/npe/src/sv-yosys/third_party/yosys/build/yosys`
- smtbmc: `/Users/npe/src/sv-yosys/third_party/yosys/build/yosys-smtbmc`, solver z3
- wall time: 6.5 s (0.01 s/case)

## Totals

- cases: 600
- agreement: 600 (quiet 141, cex 459)
- bug-class findings: 0
- determinism guard: PASS (all 600 cases ran the sim leg twice with identical summaries)

## Class C signatures (frontend/lowering gaps, deduped)

- none

## Full per-case log

| index | class | detail | wall s |
|---|---|---|---|
| 0 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 1 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 2 | agree-cex | cex at step 8 reproduced in sim | 0.11 |
| 3 | agree-cex | cex at step 2 reproduced in sim | 0.10 |
| 4 | agree-cex | cex at step 10 reproduced in sim | 0.12 |
| 5 | agree-cex | cex at step 3 reproduced in sim | 0.11 |
| 6 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 7 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 8 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 9 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 10 | agree-cex | cex at step 3 reproduced in sim | 0.10 |
| 11 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 12 | agree-cex | cex at step 10 reproduced in sim | 0.10 |
| 13 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 14 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 15 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 16 | agree-cex | cex at step 0 reproduced in sim | 0.07 |
| 17 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 18 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 19 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 20 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 21 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 22 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 23 | agree-cex | cex at step 11 reproduced in sim | 0.09 |
| 24 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 25 | agree-cex | cex at step 8 reproduced in sim | 0.10 |
| 26 | agree-cex | cex at step 11 reproduced in sim | 0.10 |
| 27 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 28 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 29 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 30 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 31 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 32 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 33 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 34 | agree-cex | cex at step 9 reproduced in sim | 0.10 |
| 35 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 36 | agree-cex | cex at step 5 reproduced in sim | 0.10 |
| 37 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 38 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 39 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 40 | agree-cex | cex at step 6 reproduced in sim | 0.08 |
| 41 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 42 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 43 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 44 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 45 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 46 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 47 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 48 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 49 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 50 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 51 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 52 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 53 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 54 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 55 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 56 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 57 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 58 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 59 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 60 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 61 | agree-cex | cex at step 9 reproduced in sim | 0.09 |
| 62 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 63 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 64 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 65 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 66 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 67 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 68 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 69 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 70 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 71 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 72 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 73 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 74 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 75 | agree-cex | cex at step 6 reproduced in sim | 0.08 |
| 76 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 77 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 78 | agree-quiet | both legs quiet to bound 12 | 0.14 |
| 79 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 80 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 81 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 82 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 83 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 84 | agree-cex | cex at step 7 reproduced in sim | 0.08 |
| 85 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 86 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 87 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 88 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 89 | agree-cex | cex at step 9 reproduced in sim | 0.08 |
| 90 | agree-cex | cex at step 6 reproduced in sim | 0.08 |
| 91 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 92 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 93 | agree-cex | cex at step 11 reproduced in sim | 0.08 |
| 94 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 95 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 96 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 97 | agree-cex | cex at step 0 reproduced in sim | 0.07 |
| 98 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 99 | agree-cex | cex at step 0 reproduced in sim | 0.07 |
| 100 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 101 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 102 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 103 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 104 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 105 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 106 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 107 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 108 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 109 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 110 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 111 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 112 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 113 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 114 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 115 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 116 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 117 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 118 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 119 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 120 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 121 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 122 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 123 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 124 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 125 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 126 | agree-cex | cex at step 7 reproduced in sim | 0.08 |
| 127 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 128 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 129 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 130 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 131 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 132 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 133 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 134 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 135 | agree-cex | cex at step 11 reproduced in sim | 0.10 |
| 136 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 137 | agree-quiet | both legs quiet to bound 12 | 0.14 |
| 138 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 139 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 140 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 141 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 142 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 143 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 144 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 145 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 146 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 147 | agree-cex | cex at step 10 reproduced in sim | 0.09 |
| 148 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 149 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 150 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 151 | agree-cex | cex at step 5 reproduced in sim | 0.29 |
| 152 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 153 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 154 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 155 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 156 | agree-cex | cex at step 7 reproduced in sim | 0.10 |
| 157 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 158 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 159 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 160 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 161 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 162 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 163 | agree-cex | cex at step 7 reproduced in sim | 0.08 |
| 164 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 165 | agree-cex | cex at step 6 reproduced in sim | 0.08 |
| 166 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 167 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 168 | agree-cex | cex at step 10 reproduced in sim | 0.17 |
| 169 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 170 | agree-cex | cex at step 9 reproduced in sim | 0.10 |
| 171 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 172 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 173 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 174 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 175 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 176 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 177 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 178 | agree-cex | cex at step 6 reproduced in sim | 0.08 |
| 179 | agree-cex | cex at step 10 reproduced in sim | 0.09 |
| 180 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 181 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 182 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 183 | agree-cex | cex at step 10 reproduced in sim | 0.09 |
| 184 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 185 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 186 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 187 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 188 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 189 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 190 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 191 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 192 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 193 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 194 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 195 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 196 | agree-quiet | both legs quiet to bound 12 | 0.12 |
| 197 | agree-cex | cex at step 7 reproduced in sim | 0.08 |
| 198 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 199 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 200 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 201 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 202 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 203 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 204 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 205 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 206 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 207 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 208 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 209 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 210 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 211 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 212 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 213 | agree-cex | cex at step 4 reproduced in sim | 0.10 |
| 214 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 215 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 216 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 217 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 218 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 219 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 220 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 221 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 222 | agree-cex | cex at step 9 reproduced in sim | 0.09 |
| 223 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 224 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 225 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 226 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 227 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 228 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 229 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 230 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 231 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 232 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 233 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 234 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 235 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 236 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 237 | agree-cex | cex at step 10 reproduced in sim | 0.09 |
| 238 | agree-cex | cex at step 6 reproduced in sim | 0.08 |
| 239 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 240 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 241 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 242 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 243 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 244 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 245 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 246 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 247 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 248 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 249 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 250 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 251 | agree-cex | cex at step 10 reproduced in sim | 0.09 |
| 252 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 253 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 254 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 255 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 256 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 257 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 258 | agree-cex | cex at step 7 reproduced in sim | 0.10 |
| 259 | agree-cex | cex at step 4 reproduced in sim | 0.12 |
| 260 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 261 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 262 | agree-cex | cex at step 9 reproduced in sim | 0.09 |
| 263 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 264 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 265 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 266 | agree-cex | cex at step 9 reproduced in sim | 0.09 |
| 267 | agree-cex | cex at step 8 reproduced in sim | 0.17 |
| 268 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 269 | agree-cex | cex at step 7 reproduced in sim | 0.08 |
| 270 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 271 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 272 | agree-cex | cex at step 10 reproduced in sim | 0.09 |
| 273 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 274 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 275 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 276 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 277 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 278 | agree-cex | cex at step 7 reproduced in sim | 0.08 |
| 279 | agree-cex | cex at step 9 reproduced in sim | 0.08 |
| 280 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 281 | agree-cex | cex at step 1 reproduced in sim | 0.07 |
| 282 | agree-cex | cex at step 10 reproduced in sim | 0.09 |
| 283 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 284 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 285 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 286 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 287 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 288 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 289 | agree-cex | cex at step 6 reproduced in sim | 0.08 |
| 290 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 291 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 292 | agree-cex | cex at step 9 reproduced in sim | 0.10 |
| 293 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 294 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 295 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 296 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 297 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 298 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 299 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 300 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 301 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 302 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 303 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 304 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 305 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 306 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 307 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 308 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 309 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 310 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 311 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 312 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 313 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 314 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 315 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 316 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 317 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 318 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 319 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 320 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 321 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 322 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 323 | agree-cex | cex at step 9 reproduced in sim | 0.09 |
| 324 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 325 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 326 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 327 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 328 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 329 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 330 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 331 | agree-cex | cex at step 7 reproduced in sim | 0.08 |
| 332 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 333 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 334 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 335 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 336 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 337 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 338 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 339 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 340 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 341 | agree-cex | cex at step 10 reproduced in sim | 0.13 |
| 342 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 343 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 344 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 345 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 346 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 347 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 348 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 349 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 350 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 351 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 352 | agree-cex | cex at step 6 reproduced in sim | 0.08 |
| 353 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 354 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 355 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 356 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 357 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 358 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 359 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 360 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 361 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 362 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 363 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 364 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 365 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 366 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 367 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 368 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 369 | agree-cex | cex at step 8 reproduced in sim | 0.08 |
| 370 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 371 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 372 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 373 | agree-quiet | both legs quiet to bound 12 | 0.12 |
| 374 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 375 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 376 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 377 | agree-cex | cex at step 9 reproduced in sim | 0.08 |
| 378 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 379 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 380 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 381 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 382 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 383 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 384 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 385 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 386 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 387 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 388 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 389 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 390 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 391 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 392 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 393 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 394 | agree-cex | cex at step 10 reproduced in sim | 0.10 |
| 395 | agree-cex | cex at step 8 reproduced in sim | 0.10 |
| 396 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 397 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 398 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 399 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 400 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 401 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 402 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 403 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 404 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 405 | agree-cex | cex at step 6 reproduced in sim | 0.10 |
| 406 | agree-cex | cex at step 4 reproduced in sim | 0.12 |
| 407 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 408 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 409 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 410 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 411 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 412 | agree-cex | cex at step 0 reproduced in sim | 0.07 |
| 413 | agree-cex | cex at step 8 reproduced in sim | 0.08 |
| 414 | agree-cex | cex at step 0 reproduced in sim | 0.07 |
| 415 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 416 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 417 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 418 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 419 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 420 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 421 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 422 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 423 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 424 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 425 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 426 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 427 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 428 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 429 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 430 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 431 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 432 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 433 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 434 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 435 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 436 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 437 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 438 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 439 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 440 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 441 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 442 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 443 | agree-cex | cex at step 7 reproduced in sim | 0.10 |
| 444 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 445 | agree-cex | cex at step 2 reproduced in sim | 0.10 |
| 446 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 447 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 448 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 449 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 450 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 451 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 452 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 453 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 454 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 455 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 456 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 457 | agree-cex | cex at step 9 reproduced in sim | 0.10 |
| 458 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 459 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 460 | agree-cex | cex at step 6 reproduced in sim | 0.08 |
| 461 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 462 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 463 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 464 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 465 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 466 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 467 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 468 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 469 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 470 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 471 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 472 | agree-quiet | both legs quiet to bound 12 | 0.15 |
| 473 | agree-cex | cex at step 10 reproduced in sim | 0.10 |
| 474 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 475 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 476 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 477 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 478 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 479 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 480 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 481 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 482 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 483 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 484 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 485 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 486 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 487 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 488 | agree-cex | cex at step 8 reproduced in sim | 0.08 |
| 489 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 490 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 491 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 492 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 493 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 494 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 495 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 496 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 497 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 498 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 499 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 500 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 501 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 502 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 503 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 504 | agree-cex | cex at step 6 reproduced in sim | 0.10 |
| 505 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 506 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 507 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 508 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 509 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 510 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 511 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 512 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 513 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 514 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 515 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 516 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 517 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 518 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 519 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 520 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 521 | agree-cex | cex at step 0 reproduced in sim | 0.07 |
| 522 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 523 | agree-cex | cex at step 6 reproduced in sim | 0.08 |
| 524 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 525 | agree-cex | cex at step 9 reproduced in sim | 0.09 |
| 526 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 527 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 528 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 529 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 530 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 531 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 532 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 533 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 534 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 535 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 536 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 537 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 538 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 539 | agree-cex | cex at step 10 reproduced in sim | 0.09 |
| 540 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 541 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 542 | agree-cex | cex at step 10 reproduced in sim | 0.10 |
| 543 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 544 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 545 | agree-cex | cex at step 10 reproduced in sim | 0.09 |
| 546 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 547 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 548 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 549 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 550 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 551 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 552 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 553 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 554 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 555 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 556 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 557 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 558 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 559 | agree-cex | cex at step 7 reproduced in sim | 0.08 |
| 560 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 561 | agree-cex | cex at step 11 reproduced in sim | 0.09 |
| 562 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 563 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 564 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 565 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 566 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 567 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 568 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 569 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 570 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 571 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 572 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 573 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 574 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 575 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 576 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 577 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 578 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 579 | agree-quiet | both legs quiet to bound 12 | 0.12 |
| 580 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 581 | agree-cex | cex at step 0 reproduced in sim | 0.07 |
| 582 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 583 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 584 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 585 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 586 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 587 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 588 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 589 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 590 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 591 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 592 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 593 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 594 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 595 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 596 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 597 | agree-cex | cex at step 11 reproduced in sim | 0.10 |
| 598 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 599 | agree-cex | cex at step 1 reproduced in sim | 0.07 |
