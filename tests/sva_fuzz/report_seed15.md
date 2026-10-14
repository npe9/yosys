# sva_fuzz report -- seed 15, 600 cases, bound 12

- yosys: `/Users/npe/src/sv-yosys/third_party/yosys/build/yosys`
- smtbmc: `/Users/npe/src/sv-yosys/third_party/yosys/build/yosys-smtbmc`, solver z3
- wall time: 6.5 s (0.01 s/case)

## Totals

- cases: 600
- agreement: 600 (quiet 132, cex 468)
- bug-class findings: 0
- determinism guard: PASS (all 600 cases ran the sim leg twice with identical summaries)

## Class C signatures (frontend/lowering gaps, deduped)

- none

## Full per-case log

| index | class | detail | wall s |
|---|---|---|---|
| 0 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 1 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 2 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 3 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 4 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 5 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 6 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 7 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 8 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 9 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 10 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 11 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 12 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 13 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 14 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 15 | agree-cex | cex at step 10 reproduced in sim | 0.10 |
| 16 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 17 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 18 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 19 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 20 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 21 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 22 | agree-cex | cex at step 10 reproduced in sim | 0.09 |
| 23 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 24 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 25 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 26 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 27 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 28 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 29 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 30 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 31 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 32 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 33 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 34 | agree-cex | cex at step 0 reproduced in sim | 0.07 |
| 35 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 36 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 37 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 38 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 39 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 40 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 41 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 42 | agree-cex | cex at step 9 reproduced in sim | 0.09 |
| 43 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 44 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 45 | agree-cex | cex at step 11 reproduced in sim | 0.09 |
| 46 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 47 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 48 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 49 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 50 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 51 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 52 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 53 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 54 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 55 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 56 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 57 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 58 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 59 | agree-cex | cex at step 8 reproduced in sim | 0.08 |
| 60 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 61 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 62 | agree-cex | cex at step 10 reproduced in sim | 0.10 |
| 63 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 64 | agree-quiet | both legs quiet to bound 12 | 0.12 |
| 65 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 66 | agree-cex | cex at step 6 reproduced in sim | 0.08 |
| 67 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 68 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 69 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 70 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 71 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 72 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 73 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 74 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 75 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 76 | agree-cex | cex at step 8 reproduced in sim | 0.13 |
| 77 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 78 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 79 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 80 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 81 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 82 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 83 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 84 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 85 | agree-cex | cex at step 6 reproduced in sim | 0.08 |
| 86 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 87 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 88 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 89 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 90 | agree-cex | cex at step 0 reproduced in sim | 0.07 |
| 91 | agree-cex | cex at step 10 reproduced in sim | 0.10 |
| 92 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 93 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 94 | agree-cex | cex at step 11 reproduced in sim | 0.10 |
| 95 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 96 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 97 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 98 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 99 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 100 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 101 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 102 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 103 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 104 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 105 | agree-cex | cex at step 9 reproduced in sim | 0.09 |
| 106 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 107 | agree-cex | cex at step 11 reproduced in sim | 0.10 |
| 108 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 109 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 110 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 111 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 112 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 113 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 114 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 115 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 116 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 117 | agree-cex | cex at step 0 reproduced in sim | 0.07 |
| 118 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 119 | agree-cex | cex at step 10 reproduced in sim | 0.10 |
| 120 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 121 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 122 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 123 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 124 | agree-cex | cex at step 8 reproduced in sim | 0.08 |
| 125 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 126 | agree-cex | cex at step 7 reproduced in sim | 0.08 |
| 127 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 128 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 129 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 130 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 131 | agree-cex | cex at step 10 reproduced in sim | 0.09 |
| 132 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 133 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 134 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 135 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 136 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 137 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 138 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 139 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 140 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 141 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 142 | agree-cex | cex at step 6 reproduced in sim | 0.08 |
| 143 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 144 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 145 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 146 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 147 | agree-cex | cex at step 10 reproduced in sim | 0.09 |
| 148 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 149 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 150 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 151 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 152 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 153 | agree-cex | cex at step 9 reproduced in sim | 0.09 |
| 154 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 155 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 156 | agree-cex | cex at step 9 reproduced in sim | 0.09 |
| 157 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 158 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 159 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 160 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 161 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 162 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 163 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 164 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 165 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 166 | agree-cex | cex at step 9 reproduced in sim | 0.08 |
| 167 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 168 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 169 | agree-cex | cex at step 6 reproduced in sim | 0.08 |
| 170 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 171 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 172 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 173 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 174 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 175 | agree-cex | cex at step 10 reproduced in sim | 0.10 |
| 176 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 177 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 178 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 179 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 180 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 181 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 182 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 183 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 184 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 185 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 186 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 187 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 188 | agree-cex | cex at step 10 reproduced in sim | 0.11 |
| 189 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 190 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 191 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 192 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 193 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 194 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 195 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 196 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 197 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 198 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 199 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 200 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 201 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 202 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 203 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 204 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 205 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 206 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 207 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 208 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 209 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 210 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 211 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 212 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 213 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 214 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 215 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 216 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 217 | agree-cex | cex at step 7 reproduced in sim | 0.08 |
| 218 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 219 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 220 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 221 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 222 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 223 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 224 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 225 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 226 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 227 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 228 | agree-cex | cex at step 9 reproduced in sim | 0.09 |
| 229 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 230 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 231 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 232 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 233 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 234 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 235 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 236 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 237 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 238 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 239 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 240 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 241 | agree-cex | cex at step 10 reproduced in sim | 0.10 |
| 242 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 243 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 244 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 245 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 246 | agree-cex | cex at step 11 reproduced in sim | 0.12 |
| 247 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 248 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 249 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 250 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 251 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 252 | agree-cex | cex at step 9 reproduced in sim | 0.10 |
| 253 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 254 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 255 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 256 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 257 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 258 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 259 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 260 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 261 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 262 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 263 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 264 | agree-cex | cex at step 10 reproduced in sim | 0.10 |
| 265 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 266 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 267 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 268 | agree-cex | cex at step 9 reproduced in sim | 0.10 |
| 269 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 270 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 271 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 272 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 273 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 274 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 275 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 276 | agree-cex | cex at step 10 reproduced in sim | 0.10 |
| 277 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 278 | agree-cex | cex at step 10 reproduced in sim | 0.10 |
| 279 | agree-cex | cex at step 5 reproduced in sim | 0.10 |
| 280 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 281 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 282 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 283 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 284 | agree-cex | cex at step 3 reproduced in sim | 0.10 |
| 285 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 286 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 287 | agree-cex | cex at step 11 reproduced in sim | 0.11 |
| 288 | agree-cex | cex at step 10 reproduced in sim | 0.10 |
| 289 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 290 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 291 | agree-cex | cex at step 11 reproduced in sim | 0.09 |
| 292 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 293 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 294 | agree-cex | cex at step 6 reproduced in sim | 0.08 |
| 295 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 296 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 297 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 298 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 299 | agree-cex | cex at step 9 reproduced in sim | 0.11 |
| 300 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 301 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 302 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 303 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 304 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 305 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 306 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 307 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 308 | agree-cex | cex at step 9 reproduced in sim | 0.11 |
| 309 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 310 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 311 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 312 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 313 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 314 | agree-cex | cex at step 9 reproduced in sim | 0.10 |
| 315 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 316 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 317 | agree-cex | cex at step 11 reproduced in sim | 0.10 |
| 318 | agree-cex | cex at step 11 reproduced in sim | 0.10 |
| 319 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 320 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 321 | agree-cex | cex at step 11 reproduced in sim | 0.09 |
| 322 | agree-cex | cex at step 7 reproduced in sim | 0.11 |
| 323 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 324 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 325 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 326 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 327 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 328 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 329 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 330 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 331 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 332 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 333 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 334 | agree-cex | cex at step 10 reproduced in sim | 0.08 |
| 335 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 336 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 337 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 338 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 339 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 340 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 341 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 342 | agree-cex | cex at step 10 reproduced in sim | 0.09 |
| 343 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 344 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 345 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 346 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 347 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 348 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 349 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 350 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 351 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 352 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 353 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 354 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 355 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 356 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 357 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 358 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 359 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 360 | agree-cex | cex at step 11 reproduced in sim | 0.10 |
| 361 | agree-cex | cex at step 8 reproduced in sim | 0.08 |
| 362 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 363 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 364 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 365 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 366 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 367 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 368 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 369 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 370 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 371 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 372 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 373 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 374 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 375 | agree-quiet | both legs quiet to bound 12 | 0.12 |
| 376 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 377 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 378 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 379 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 380 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 381 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 382 | agree-cex | cex at step 10 reproduced in sim | 0.08 |
| 383 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 384 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 385 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 386 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 387 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 388 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 389 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 390 | agree-cex | cex at step 8 reproduced in sim | 0.10 |
| 391 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 392 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 393 | agree-cex | cex at step 6 reproduced in sim | 0.10 |
| 394 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 395 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 396 | agree-cex | cex at step 6 reproduced in sim | 0.12 |
| 397 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 398 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 399 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 400 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 401 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 402 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 403 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 404 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 405 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 406 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 407 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 408 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 409 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 410 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 411 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 412 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 413 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 414 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 415 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 416 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 417 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 418 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 419 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 420 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 421 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 422 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 423 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 424 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 425 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 426 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 427 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 428 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 429 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 430 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 431 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 432 | agree-cex | cex at step 9 reproduced in sim | 0.10 |
| 433 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 434 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 435 | agree-cex | cex at step 9 reproduced in sim | 0.10 |
| 436 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 437 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 438 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 439 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 440 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 441 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 442 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 443 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 444 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 445 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 446 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 447 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 448 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 449 | agree-cex | cex at step 11 reproduced in sim | 0.09 |
| 450 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 451 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 452 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 453 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 454 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 455 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 456 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 457 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 458 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 459 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 460 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 461 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 462 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 463 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 464 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 465 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 466 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 467 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 468 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 469 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 470 | agree-cex | cex at step 9 reproduced in sim | 0.09 |
| 471 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 472 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 473 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 474 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 475 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 476 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 477 | agree-cex | cex at step 8 reproduced in sim | 0.10 |
| 478 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 479 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 480 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 481 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 482 | agree-cex | cex at step 4 reproduced in sim | 0.10 |
| 483 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 484 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 485 | agree-cex | cex at step 9 reproduced in sim | 0.09 |
| 486 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 487 | agree-cex | cex at step 9 reproduced in sim | 0.09 |
| 488 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 489 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 490 | agree-cex | cex at step 9 reproduced in sim | 0.10 |
| 491 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 492 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 493 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 494 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 495 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 496 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 497 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 498 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 499 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 500 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 501 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 502 | agree-cex | cex at step 9 reproduced in sim | 0.09 |
| 503 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 504 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 505 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 506 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 507 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 508 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 509 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 510 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 511 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 512 | agree-cex | cex at step 6 reproduced in sim | 0.08 |
| 513 | agree-cex | cex at step 8 reproduced in sim | 0.08 |
| 514 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 515 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 516 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 517 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 518 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 519 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 520 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 521 | agree-cex | cex at step 8 reproduced in sim | 0.08 |
| 522 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 523 | agree-cex | cex at step 9 reproduced in sim | 0.10 |
| 524 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 525 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 526 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 527 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 528 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 529 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 530 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 531 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 532 | agree-cex | cex at step 7 reproduced in sim | 0.08 |
| 533 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 534 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 535 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 536 | agree-cex | cex at step 6 reproduced in sim | 0.08 |
| 537 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 538 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 539 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 540 | agree-cex | cex at step 0 reproduced in sim | 0.07 |
| 541 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 542 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 543 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 544 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 545 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 546 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 547 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 548 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 549 | agree-cex | cex at step 10 reproduced in sim | 0.09 |
| 550 | agree-cex | cex at step 10 reproduced in sim | 0.09 |
| 551 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 552 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 553 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 554 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 555 | agree-cex | cex at step 8 reproduced in sim | 0.08 |
| 556 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 557 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 558 | agree-cex | cex at step 11 reproduced in sim | 0.09 |
| 559 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 560 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 561 | agree-cex | cex at step 6 reproduced in sim | 0.10 |
| 562 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 563 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 564 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 565 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 566 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 567 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 568 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 569 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 570 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 571 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 572 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 573 | agree-cex | cex at step 11 reproduced in sim | 0.09 |
| 574 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 575 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 576 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 577 | agree-cex | cex at step 10 reproduced in sim | 0.10 |
| 578 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 579 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 580 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 581 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 582 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 583 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 584 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 585 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 586 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 587 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 588 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 589 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 590 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 591 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 592 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 593 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 594 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 595 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 596 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 597 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 598 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 599 | agree-cex | cex at step 2 reproduced in sim | 0.07 |
