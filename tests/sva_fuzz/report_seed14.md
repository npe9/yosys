# sva_fuzz report -- seed 14, 600 cases, bound 12

- yosys: `/Users/npe/src/sv-yosys/third_party/yosys/build/yosys`
- smtbmc: `/Users/npe/src/sv-yosys/third_party/yosys/build/yosys-smtbmc`, solver z3
- wall time: 6.7 s (0.01 s/case)

## Totals

- cases: 600
- agreement: 600 (quiet 126, cex 474)
- bug-class findings: 0
- determinism guard: PASS (all 600 cases ran the sim leg twice with identical summaries)

## Class C signatures (frontend/lowering gaps, deduped)

- none

## Full per-case log

| index | class | detail | wall s |
|---|---|---|---|
| 0 | agree-quiet | both legs quiet to bound 12 | 0.13 |
| 1 | agree-cex | cex at step 0 reproduced in sim | 0.13 |
| 2 | agree-cex | cex at step 2 reproduced in sim | 0.13 |
| 3 | agree-cex | cex at step 5 reproduced in sim | 0.14 |
| 4 | agree-quiet | both legs quiet to bound 12 | 0.12 |
| 5 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 6 | agree-cex | cex at step 8 reproduced in sim | 0.14 |
| 7 | agree-cex | cex at step 0 reproduced in sim | 0.13 |
| 8 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 9 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 10 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 11 | agree-cex | cex at step 4 reproduced in sim | 0.10 |
| 12 | agree-cex | cex at step 4 reproduced in sim | 0.10 |
| 13 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 14 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 15 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 16 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 17 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 18 | agree-cex | cex at step 8 reproduced in sim | 0.10 |
| 19 | agree-cex | cex at step 2 reproduced in sim | 0.10 |
| 20 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 21 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 22 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 23 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 24 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 25 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 26 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 27 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 28 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 29 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 30 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 31 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 32 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 33 | agree-cex | cex at step 9 reproduced in sim | 0.09 |
| 34 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 35 | agree-cex | cex at step 8 reproduced in sim | 0.10 |
| 36 | agree-cex | cex at step 5 reproduced in sim | 0.10 |
| 37 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 38 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 39 | agree-cex | cex at step 8 reproduced in sim | 0.11 |
| 40 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 41 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 42 | agree-cex | cex at step 10 reproduced in sim | 0.09 |
| 43 | agree-cex | cex at step 9 reproduced in sim | 0.10 |
| 44 | agree-cex | cex at step 10 reproduced in sim | 0.09 |
| 45 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 46 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 47 | agree-cex | cex at step 4 reproduced in sim | 0.10 |
| 48 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 49 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 50 | agree-cex | cex at step 7 reproduced in sim | 0.10 |
| 51 | agree-cex | cex at step 4 reproduced in sim | 0.10 |
| 52 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 53 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 54 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 55 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 56 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 57 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 58 | agree-cex | cex at step 1 reproduced in sim | 0.10 |
| 59 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 60 | agree-quiet | both legs quiet to bound 12 | 0.13 |
| 61 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 62 | agree-quiet | both legs quiet to bound 12 | 0.13 |
| 63 | agree-cex | cex at step 11 reproduced in sim | 0.12 |
| 64 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 65 | agree-cex | cex at step 2 reproduced in sim | 0.11 |
| 66 | agree-cex | cex at step 6 reproduced in sim | 0.11 |
| 67 | agree-cex | cex at step 2 reproduced in sim | 0.10 |
| 68 | agree-cex | cex at step 8 reproduced in sim | 0.10 |
| 69 | agree-cex | cex at step 1 reproduced in sim | 0.10 |
| 70 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 71 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 72 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 73 | agree-cex | cex at step 11 reproduced in sim | 0.10 |
| 74 | agree-cex | cex at step 11 reproduced in sim | 0.11 |
| 75 | agree-cex | cex at step 4 reproduced in sim | 0.10 |
| 76 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 77 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 78 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 79 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 80 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 81 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 82 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 83 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 84 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 85 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 86 | agree-cex | cex at step 3 reproduced in sim | 0.10 |
| 87 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 88 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 89 | agree-cex | cex at step 8 reproduced in sim | 0.10 |
| 90 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 91 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 92 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 93 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 94 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 95 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 96 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 97 | agree-quiet | both legs quiet to bound 12 | 0.12 |
| 98 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 99 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 100 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 101 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 102 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 103 | agree-cex | cex at step 5 reproduced in sim | 0.10 |
| 104 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 105 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 106 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 107 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 108 | agree-cex | cex at step 4 reproduced in sim | 0.11 |
| 109 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 110 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 111 | agree-quiet | both legs quiet to bound 12 | 0.12 |
| 112 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 113 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 114 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 115 | agree-cex | cex at step 8 reproduced in sim | 0.10 |
| 116 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 117 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 118 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 119 | agree-quiet | both legs quiet to bound 12 | 0.16 |
| 120 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 121 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 122 | agree-cex | cex at step 10 reproduced in sim | 0.09 |
| 123 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 124 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 125 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 126 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 127 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 128 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 129 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 130 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 131 | agree-cex | cex at step 2 reproduced in sim | 0.10 |
| 132 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 133 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 134 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 135 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 136 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 137 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 138 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 139 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 140 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 141 | agree-cex | cex at step 10 reproduced in sim | 0.09 |
| 142 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 143 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 144 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 145 | agree-cex | cex at step 6 reproduced in sim | 0.10 |
| 146 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 147 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 148 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 149 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 150 | agree-cex | cex at step 9 reproduced in sim | 0.10 |
| 151 | agree-cex | cex at step 9 reproduced in sim | 0.09 |
| 152 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 153 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 154 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 155 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 156 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 157 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 158 | agree-cex | cex at step 9 reproduced in sim | 0.10 |
| 159 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 160 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 161 | agree-cex | cex at step 3 reproduced in sim | 0.10 |
| 162 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 163 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 164 | agree-cex | cex at step 10 reproduced in sim | 0.10 |
| 165 | agree-cex | cex at step 11 reproduced in sim | 0.09 |
| 166 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 167 | agree-cex | cex at step 10 reproduced in sim | 0.11 |
| 168 | agree-cex | cex at step 7 reproduced in sim | 0.11 |
| 169 | agree-cex | cex at step 6 reproduced in sim | 0.08 |
| 170 | agree-cex | cex at step 11 reproduced in sim | 0.11 |
| 171 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 172 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 173 | agree-cex | cex at step 8 reproduced in sim | 0.10 |
| 174 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 175 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 176 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 177 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 178 | agree-cex | cex at step 11 reproduced in sim | 0.10 |
| 179 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 180 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 181 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 182 | agree-cex | cex at step 1 reproduced in sim | 0.07 |
| 183 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 184 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 185 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 186 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 187 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 188 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 189 | agree-cex | cex at step 8 reproduced in sim | 0.10 |
| 190 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 191 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 192 | agree-cex | cex at step 10 reproduced in sim | 0.09 |
| 193 | agree-cex | cex at step 9 reproduced in sim | 0.09 |
| 194 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 195 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 196 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 197 | agree-cex | cex at step 6 reproduced in sim | 0.08 |
| 198 | agree-cex | cex at step 10 reproduced in sim | 0.09 |
| 199 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 200 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 201 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 202 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 203 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 204 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 205 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 206 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 207 | agree-cex | cex at step 3 reproduced in sim | 0.11 |
| 208 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 209 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 210 | agree-cex | cex at step 0 reproduced in sim | 0.07 |
| 211 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 212 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 213 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 214 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 215 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 216 | agree-cex | cex at step 9 reproduced in sim | 0.10 |
| 217 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 218 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 219 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 220 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 221 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 222 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 223 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 224 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 225 | agree-cex | cex at step 11 reproduced in sim | 0.09 |
| 226 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 227 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 228 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 229 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 230 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 231 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 232 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 233 | agree-cex | cex at step 8 reproduced in sim | 0.10 |
| 234 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 235 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 236 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 237 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 238 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 239 | agree-cex | cex at step 0 reproduced in sim | 0.07 |
| 240 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 241 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 242 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 243 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 244 | agree-cex | cex at step 3 reproduced in sim | 0.10 |
| 245 | agree-cex | cex at step 9 reproduced in sim | 0.10 |
| 246 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 247 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 248 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 249 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 250 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 251 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 252 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 253 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 254 | agree-cex | cex at step 11 reproduced in sim | 0.10 |
| 255 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 256 | agree-cex | cex at step 10 reproduced in sim | 0.10 |
| 257 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 258 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 259 | agree-cex | cex at step 7 reproduced in sim | 0.08 |
| 260 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 261 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 262 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 263 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 264 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 265 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 266 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 267 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 268 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 269 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 270 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 271 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 272 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 273 | agree-cex | cex at step 9 reproduced in sim | 0.08 |
| 274 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 275 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 276 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 277 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 278 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 279 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 280 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 281 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 282 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 283 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 284 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 285 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 286 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 287 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 288 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 289 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 290 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 291 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 292 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 293 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 294 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 295 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 296 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 297 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 298 | agree-cex | cex at step 9 reproduced in sim | 0.10 |
| 299 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 300 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 301 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 302 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 303 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 304 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 305 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 306 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 307 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 308 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 309 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 310 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 311 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 312 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 313 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 314 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 315 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 316 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 317 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 318 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 319 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 320 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 321 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 322 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 323 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 324 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 325 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 326 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 327 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 328 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 329 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 330 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 331 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 332 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 333 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 334 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 335 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 336 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 337 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 338 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 339 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 340 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 341 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 342 | agree-cex | cex at step 11 reproduced in sim | 0.09 |
| 343 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 344 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 345 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 346 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 347 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 348 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 349 | agree-cex | cex at step 10 reproduced in sim | 0.10 |
| 350 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 351 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 352 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 353 | agree-cex | cex at step 10 reproduced in sim | 0.11 |
| 354 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 355 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 356 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 357 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 358 | agree-cex | cex at step 5 reproduced in sim | 0.10 |
| 359 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 360 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 361 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 362 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 363 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 364 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 365 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 366 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 367 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 368 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 369 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 370 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 371 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 372 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 373 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 374 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 375 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 376 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 377 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 378 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 379 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 380 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 381 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 382 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 383 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 384 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 385 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 386 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 387 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 388 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 389 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 390 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 391 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 392 | agree-cex | cex at step 9 reproduced in sim | 0.09 |
| 393 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 394 | agree-cex | cex at step 7 reproduced in sim | 0.10 |
| 395 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 396 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 397 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 398 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 399 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 400 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 401 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 402 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 403 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 404 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 405 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 406 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 407 | agree-quiet | both legs quiet to bound 12 | 0.13 |
| 408 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 409 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 410 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 411 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 412 | agree-cex | cex at step 7 reproduced in sim | 0.08 |
| 413 | agree-cex | cex at step 7 reproduced in sim | 0.08 |
| 414 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 415 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 416 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 417 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 418 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 419 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 420 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 421 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 422 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 423 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 424 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 425 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 426 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 427 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 428 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 429 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 430 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 431 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 432 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 433 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 434 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 435 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 436 | agree-cex | cex at step 9 reproduced in sim | 0.11 |
| 437 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 438 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 439 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 440 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 441 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 442 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 443 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 444 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 445 | agree-cex | cex at step 10 reproduced in sim | 0.09 |
| 446 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 447 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 448 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 449 | agree-cex | cex at step 6 reproduced in sim | 0.08 |
| 450 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 451 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 452 | agree-cex | cex at step 8 reproduced in sim | 0.10 |
| 453 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 454 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 455 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 456 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 457 | agree-cex | cex at step 9 reproduced in sim | 0.16 |
| 458 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 459 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 460 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 461 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 462 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 463 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 464 | agree-cex | cex at step 10 reproduced in sim | 0.11 |
| 465 | agree-cex | cex at step 0 reproduced in sim | 0.11 |
| 466 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 467 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 468 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 469 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 470 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 471 | agree-cex | cex at step 11 reproduced in sim | 0.09 |
| 472 | agree-cex | cex at step 6 reproduced in sim | 0.08 |
| 473 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 474 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 475 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 476 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 477 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 478 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 479 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 480 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 481 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 482 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 483 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 484 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 485 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 486 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 487 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 488 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 489 | agree-cex | cex at step 0 reproduced in sim | 0.07 |
| 490 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 491 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 492 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 493 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 494 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 495 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 496 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 497 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 498 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 499 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 500 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 501 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 502 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 503 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 504 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 505 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 506 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 507 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 508 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 509 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 510 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 511 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 512 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 513 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 514 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 515 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 516 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 517 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 518 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 519 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 520 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 521 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 522 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 523 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 524 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 525 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 526 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 527 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 528 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 529 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 530 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 531 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 532 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 533 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 534 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 535 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 536 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 537 | agree-cex | cex at step 9 reproduced in sim | 0.08 |
| 538 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 539 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 540 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 541 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 542 | agree-cex | cex at step 10 reproduced in sim | 0.10 |
| 543 | agree-cex | cex at step 10 reproduced in sim | 0.08 |
| 544 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 545 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 546 | agree-cex | cex at step 6 reproduced in sim | 0.08 |
| 547 | agree-cex | cex at step 10 reproduced in sim | 0.09 |
| 548 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 549 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 550 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 551 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 552 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 553 | agree-cex | cex at step 10 reproduced in sim | 0.10 |
| 554 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 555 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 556 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 557 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 558 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 559 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 560 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 561 | agree-cex | cex at step 11 reproduced in sim | 0.10 |
| 562 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 563 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 564 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 565 | agree-cex | cex at step 11 reproduced in sim | 0.13 |
| 566 | agree-cex | cex at step 9 reproduced in sim | 0.09 |
| 567 | agree-cex | cex at step 9 reproduced in sim | 0.09 |
| 568 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 569 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 570 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 571 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 572 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 573 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 574 | agree-cex | cex at step 11 reproduced in sim | 0.08 |
| 575 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 576 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 577 | agree-cex | cex at step 10 reproduced in sim | 0.10 |
| 578 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 579 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 580 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 581 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 582 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 583 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 584 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 585 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 586 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 587 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 588 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 589 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 590 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 591 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 592 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 593 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 594 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 595 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 596 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 597 | agree-cex | cex at step 0 reproduced in sim | 0.07 |
| 598 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 599 | agree-cex | cex at step 0 reproduced in sim | 0.07 |
