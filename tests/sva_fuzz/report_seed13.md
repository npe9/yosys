# sva_fuzz report -- seed 13, 600 cases, bound 12

- yosys: `/Users/npe/src/sv-yosys/third_party/yosys/build/yosys`
- smtbmc: `/Users/npe/src/sv-yosys/third_party/yosys/build/yosys-smtbmc`, solver z3
- wall time: 7.0 s (0.01 s/case)

## Totals

- cases: 600
- agreement: 600 (quiet 139, cex 461)
- bug-class findings: 0
- determinism guard: PASS (all 600 cases ran the sim leg twice with identical summaries)

## Class C signatures (frontend/lowering gaps, deduped)

- none

## Full per-case log

| index | class | detail | wall s |
|---|---|---|---|
| 0 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 1 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 2 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 3 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 4 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 5 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 6 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 7 | agree-cex | cex at step 3 reproduced in sim | 0.10 |
| 8 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 9 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 10 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 11 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 12 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 13 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 14 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 15 | agree-cex | cex at step 11 reproduced in sim | 0.10 |
| 16 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 17 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 18 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 19 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 20 | agree-cex | cex at step 11 reproduced in sim | 0.10 |
| 21 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 22 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 23 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 24 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 25 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 26 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 27 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 28 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 29 | agree-cex | cex at step 10 reproduced in sim | 0.11 |
| 30 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 31 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 32 | agree-cex | cex at step 9 reproduced in sim | 0.10 |
| 33 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 34 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 35 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 36 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 37 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 38 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 39 | agree-cex | cex at step 11 reproduced in sim | 0.09 |
| 40 | agree-cex | cex at step 6 reproduced in sim | 0.08 |
| 41 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 42 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 43 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 44 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 45 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 46 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 47 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 48 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 49 | agree-cex | cex at step 9 reproduced in sim | 0.09 |
| 50 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 51 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 52 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 53 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 54 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 55 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 56 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 57 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 58 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 59 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 60 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 61 | agree-cex | cex at step 10 reproduced in sim | 0.11 |
| 62 | agree-cex | cex at step 10 reproduced in sim | 0.09 |
| 63 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 64 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 65 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 66 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 67 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 68 | agree-cex | cex at step 11 reproduced in sim | 0.14 |
| 69 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 70 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 71 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 72 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 73 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 74 | agree-cex | cex at step 9 reproduced in sim | 0.09 |
| 75 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 76 | agree-cex | cex at step 6 reproduced in sim | 0.08 |
| 77 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 78 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 79 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 80 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 81 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 82 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 83 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 84 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 85 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 86 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 87 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 88 | agree-cex | cex at step 11 reproduced in sim | 0.09 |
| 89 | agree-cex | cex at step 10 reproduced in sim | 0.09 |
| 90 | agree-cex | cex at step 2 reproduced in sim | 0.07 |
| 91 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 92 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 93 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 94 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 95 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 96 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 97 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 98 | agree-cex | cex at step 11 reproduced in sim | 0.10 |
| 99 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 100 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 101 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 102 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 103 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 104 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 105 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 106 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 107 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 108 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 109 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 110 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 111 | agree-cex | cex at step 8 reproduced in sim | 0.08 |
| 112 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 113 | agree-cex | cex at step 9 reproduced in sim | 0.10 |
| 114 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 115 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 116 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 117 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 118 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 119 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 120 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 121 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 122 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 123 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 124 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 125 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 126 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 127 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 128 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 129 | agree-cex | cex at step 3 reproduced in sim | 0.11 |
| 130 | agree-cex | cex at step 5 reproduced in sim | 0.11 |
| 131 | agree-cex | cex at step 1 reproduced in sim | 0.10 |
| 132 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 133 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 134 | agree-cex | cex at step 8 reproduced in sim | 0.08 |
| 135 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 136 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 137 | agree-cex | cex at step 4 reproduced in sim | 0.10 |
| 138 | agree-cex | cex at step 10 reproduced in sim | 0.11 |
| 139 | agree-cex | cex at step 10 reproduced in sim | 0.10 |
| 140 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 141 | agree-cex | cex at step 3 reproduced in sim | 0.10 |
| 142 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 143 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 144 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 145 | agree-cex | cex at step 8 reproduced in sim | 0.11 |
| 146 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 147 | agree-cex | cex at step 2 reproduced in sim | 0.10 |
| 148 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 149 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 150 | agree-cex | cex at step 4 reproduced in sim | 0.10 |
| 151 | agree-cex | cex at step 7 reproduced in sim | 0.10 |
| 152 | agree-quiet | both legs quiet to bound 12 | 0.12 |
| 153 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 154 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 155 | agree-cex | cex at step 7 reproduced in sim | 0.10 |
| 156 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 157 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 158 | agree-cex | cex at step 1 reproduced in sim | 0.10 |
| 159 | agree-cex | cex at step 1 reproduced in sim | 0.10 |
| 160 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 161 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 162 | agree-cex | cex at step 2 reproduced in sim | 0.10 |
| 163 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 164 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 165 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 166 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 167 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 168 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 169 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 170 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 171 | agree-cex | cex at step 10 reproduced in sim | 0.10 |
| 172 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 173 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 174 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 175 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 176 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 177 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 178 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 179 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 180 | agree-cex | cex at step 10 reproduced in sim | 0.12 |
| 181 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 182 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 183 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 184 | agree-cex | cex at step 5 reproduced in sim | 0.10 |
| 185 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 186 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 187 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 188 | agree-cex | cex at step 10 reproduced in sim | 0.09 |
| 189 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 190 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 191 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 192 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 193 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 194 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 195 | agree-cex | cex at step 8 reproduced in sim | 0.10 |
| 196 | agree-cex | cex at step 6 reproduced in sim | 0.10 |
| 197 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 198 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 199 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 200 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 201 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 202 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 203 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 204 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 205 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 206 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 207 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 208 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 209 | agree-cex | cex at step 5 reproduced in sim | 0.10 |
| 210 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 211 | agree-cex | cex at step 9 reproduced in sim | 0.11 |
| 212 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 213 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 214 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 215 | agree-cex | cex at step 4 reproduced in sim | 0.10 |
| 216 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 217 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 218 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 219 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 220 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 221 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 222 | agree-cex | cex at step 6 reproduced in sim | 0.10 |
| 223 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 224 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 225 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 226 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 227 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 228 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 229 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 230 | agree-cex | cex at step 7 reproduced in sim | 0.10 |
| 231 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 232 | agree-cex | cex at step 5 reproduced in sim | 0.10 |
| 233 | agree-cex | cex at step 9 reproduced in sim | 0.10 |
| 234 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 235 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 236 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 237 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 238 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 239 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 240 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 241 | agree-cex | cex at step 6 reproduced in sim | 0.10 |
| 242 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 243 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 244 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 245 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 246 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 247 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 248 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 249 | agree-cex | cex at step 3 reproduced in sim | 0.10 |
| 250 | agree-cex | cex at step 3 reproduced in sim | 0.10 |
| 251 | agree-cex | cex at step 1 reproduced in sim | 0.10 |
| 252 | agree-cex | cex at step 1 reproduced in sim | 0.10 |
| 253 | agree-cex | cex at step 8 reproduced in sim | 0.11 |
| 254 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 255 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 256 | agree-cex | cex at step 2 reproduced in sim | 0.12 |
| 257 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 258 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 259 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 260 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 261 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 262 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 263 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 264 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 265 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 266 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 267 | agree-cex | cex at step 10 reproduced in sim | 0.11 |
| 268 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 269 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 270 | agree-cex | cex at step 4 reproduced in sim | 0.10 |
| 271 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 272 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 273 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 274 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 275 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 276 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 277 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 278 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 279 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 280 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 281 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 282 | agree-cex | cex at step 7 reproduced in sim | 0.10 |
| 283 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 284 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 285 | agree-cex | cex at step 9 reproduced in sim | 0.11 |
| 286 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 287 | agree-cex | cex at step 5 reproduced in sim | 0.10 |
| 288 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 289 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 290 | agree-cex | cex at step 9 reproduced in sim | 0.10 |
| 291 | agree-cex | cex at step 7 reproduced in sim | 0.10 |
| 292 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 293 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 294 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 295 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 296 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 297 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 298 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 299 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 300 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 301 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 302 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 303 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 304 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 305 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 306 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 307 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 308 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 309 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 310 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 311 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 312 | agree-cex | cex at step 10 reproduced in sim | 0.09 |
| 313 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 314 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 315 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 316 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 317 | agree-cex | cex at step 5 reproduced in sim | 0.11 |
| 318 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 319 | agree-cex | cex at step 2 reproduced in sim | 0.10 |
| 320 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 321 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 322 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 323 | agree-cex | cex at step 10 reproduced in sim | 0.12 |
| 324 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 325 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 326 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 327 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 328 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 329 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 330 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 331 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 332 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 333 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 334 | agree-cex | cex at step 4 reproduced in sim | 0.10 |
| 335 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 336 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 337 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 338 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 339 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 340 | agree-cex | cex at step 9 reproduced in sim | 0.10 |
| 341 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 342 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 343 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 344 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 345 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 346 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 347 | agree-cex | cex at step 8 reproduced in sim | 0.10 |
| 348 | agree-cex | cex at step 8 reproduced in sim | 0.10 |
| 349 | agree-cex | cex at step 10 reproduced in sim | 0.10 |
| 350 | agree-cex | cex at step 6 reproduced in sim | 0.10 |
| 351 | agree-cex | cex at step 1 reproduced in sim | 0.10 |
| 352 | agree-cex | cex at step 6 reproduced in sim | 0.10 |
| 353 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 354 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 355 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 356 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 357 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 358 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 359 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 360 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 361 | agree-cex | cex at step 7 reproduced in sim | 0.10 |
| 362 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 363 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 364 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 365 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 366 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 367 | agree-cex | cex at step 8 reproduced in sim | 0.10 |
| 368 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 369 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 370 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 371 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 372 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 373 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 374 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 375 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 376 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 377 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 378 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 379 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 380 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 381 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 382 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 383 | agree-cex | cex at step 5 reproduced in sim | 0.10 |
| 384 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 385 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 386 | agree-cex | cex at step 5 reproduced in sim | 0.10 |
| 387 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 388 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 389 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 390 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 391 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 392 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 393 | agree-cex | cex at step 10 reproduced in sim | 0.12 |
| 394 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 395 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 396 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 397 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 398 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 399 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 400 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 401 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 402 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 403 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 404 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 405 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 406 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 407 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 408 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 409 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 410 | agree-cex | cex at step 4 reproduced in sim | 0.10 |
| 411 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 412 | agree-cex | cex at step 8 reproduced in sim | 0.10 |
| 413 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 414 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 415 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 416 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 417 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 418 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 419 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 420 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 421 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 422 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 423 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 424 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 425 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 426 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 427 | agree-cex | cex at step 8 reproduced in sim | 0.10 |
| 428 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 429 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 430 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 431 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 432 | agree-cex | cex at step 1 reproduced in sim | 0.10 |
| 433 | agree-cex | cex at step 10 reproduced in sim | 0.09 |
| 434 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 435 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 436 | agree-cex | cex at step 2 reproduced in sim | 0.10 |
| 437 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 438 | agree-cex | cex at step 6 reproduced in sim | 0.10 |
| 439 | agree-cex | cex at step 1 reproduced in sim | 0.10 |
| 440 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 441 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 442 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 443 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 444 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 445 | agree-cex | cex at step 11 reproduced in sim | 0.15 |
| 446 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 447 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 448 | agree-cex | cex at step 8 reproduced in sim | 0.10 |
| 449 | agree-cex | cex at step 8 reproduced in sim | 0.10 |
| 450 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 451 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 452 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 453 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 454 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 455 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 456 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 457 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 458 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 459 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 460 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 461 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 462 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 463 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 464 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 465 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 466 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 467 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 468 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 469 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 470 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 471 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 472 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 473 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 474 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 475 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 476 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 477 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 478 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 479 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 480 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 481 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 482 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 483 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 484 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 485 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 486 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 487 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 488 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 489 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 490 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 491 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 492 | agree-cex | cex at step 7 reproduced in sim | 0.08 |
| 493 | agree-cex | cex at step 11 reproduced in sim | 0.17 |
| 494 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 495 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 496 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 497 | agree-cex | cex at step 3 reproduced in sim | 0.10 |
| 498 | agree-cex | cex at step 3 reproduced in sim | 0.10 |
| 499 | agree-cex | cex at step 2 reproduced in sim | 0.10 |
| 500 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 501 | agree-cex | cex at step 7 reproduced in sim | 0.10 |
| 502 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 503 | agree-cex | cex at step 6 reproduced in sim | 0.11 |
| 504 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 505 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 506 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 507 | agree-cex | cex at step 1 reproduced in sim | 0.10 |
| 508 | agree-cex | cex at step 7 reproduced in sim | 0.11 |
| 509 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 510 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 511 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 512 | agree-cex | cex at step 7 reproduced in sim | 0.10 |
| 513 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 514 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 515 | agree-cex | cex at step 1 reproduced in sim | 0.10 |
| 516 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 517 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 518 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 519 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 520 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 521 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 522 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 523 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 524 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 525 | agree-cex | cex at step 3 reproduced in sim | 0.10 |
| 526 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 527 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 528 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 529 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 530 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 531 | agree-cex | cex at step 7 reproduced in sim | 0.10 |
| 532 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 533 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 534 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 535 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 536 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 537 | agree-cex | cex at step 7 reproduced in sim | 0.10 |
| 538 | agree-cex | cex at step 11 reproduced in sim | 0.10 |
| 539 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 540 | agree-cex | cex at step 6 reproduced in sim | 0.11 |
| 541 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 542 | agree-cex | cex at step 10 reproduced in sim | 0.16 |
| 543 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 544 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 545 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 546 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 547 | agree-cex | cex at step 11 reproduced in sim | 0.10 |
| 548 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 549 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 550 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 551 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 552 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 553 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 554 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 555 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 556 | agree-cex | cex at step 11 reproduced in sim | 0.11 |
| 557 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 558 | agree-cex | cex at step 8 reproduced in sim | 0.10 |
| 559 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 560 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 561 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 562 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 563 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 564 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 565 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 566 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 567 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 568 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 569 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 570 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 571 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 572 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 573 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 574 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 575 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 576 | agree-quiet | both legs quiet to bound 12 | 0.12 |
| 577 | agree-cex | cex at step 5 reproduced in sim | 0.10 |
| 578 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 579 | agree-cex | cex at step 9 reproduced in sim | 0.11 |
| 580 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 581 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 582 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 583 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 584 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 585 | agree-cex | cex at step 1 reproduced in sim | 0.10 |
| 586 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 587 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 588 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 589 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 590 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 591 | agree-cex | cex at step 4 reproduced in sim | 0.10 |
| 592 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 593 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 594 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 595 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 596 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 597 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 598 | agree-cex | cex at step 8 reproduced in sim | 0.08 |
| 599 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
