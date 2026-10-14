# sva_fuzz report -- seed 16, 600 cases, bound 12

- yosys: `/Users/npe/src/sv-yosys/third_party/yosys/build/yosys`
- smtbmc: `/Users/npe/src/sv-yosys/third_party/yosys/build/yosys-smtbmc`, solver z3
- wall time: 6.6 s (0.01 s/case)

## Totals

- cases: 600
- agreement: 600 (quiet 121, cex 479)
- bug-class findings: 0
- determinism guard: PASS (all 600 cases ran the sim leg twice with identical summaries)

## Class C signatures (frontend/lowering gaps, deduped)

- none

## Full per-case log

| index | class | detail | wall s |
|---|---|---|---|
| 0 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 1 | agree-cex | cex at step 2 reproduced in sim | 0.10 |
| 2 | agree-cex | cex at step 8 reproduced in sim | 0.10 |
| 3 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 4 | agree-cex | cex at step 8 reproduced in sim | 0.10 |
| 5 | agree-cex | cex at step 1 reproduced in sim | 0.10 |
| 6 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 7 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 8 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 9 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 10 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 11 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 12 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 13 | agree-cex | cex at step 8 reproduced in sim | 0.10 |
| 14 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 15 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 16 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 17 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 18 | agree-cex | cex at step 10 reproduced in sim | 0.10 |
| 19 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 20 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 21 | agree-cex | cex at step 4 reproduced in sim | 0.10 |
| 22 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 23 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 24 | agree-cex | cex at step 6 reproduced in sim | 0.10 |
| 25 | agree-cex | cex at step 9 reproduced in sim | 0.10 |
| 26 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 27 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 28 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 29 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 30 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 31 | agree-cex | cex at step 3 reproduced in sim | 0.10 |
| 32 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 33 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 34 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 35 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 36 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 37 | agree-cex | cex at step 10 reproduced in sim | 0.10 |
| 38 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 39 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 40 | agree-cex | cex at step 9 reproduced in sim | 0.09 |
| 41 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 42 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 43 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 44 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 45 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 46 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 47 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 48 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 49 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 50 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 51 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 52 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 53 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 54 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 55 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 56 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 57 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 58 | agree-cex | cex at step 7 reproduced in sim | 0.11 |
| 59 | agree-cex | cex at step 7 reproduced in sim | 0.08 |
| 60 | agree-cex | cex at step 11 reproduced in sim | 0.10 |
| 61 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 62 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 63 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 64 | agree-cex | cex at step 10 reproduced in sim | 0.09 |
| 65 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 66 | agree-cex | cex at step 8 reproduced in sim | 0.08 |
| 67 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 68 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 69 | agree-cex | cex at step 7 reproduced in sim | 0.08 |
| 70 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 71 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 72 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 73 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 74 | agree-cex | cex at step 9 reproduced in sim | 0.11 |
| 75 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 76 | agree-cex | cex at step 6 reproduced in sim | 0.08 |
| 77 | agree-cex | cex at step 9 reproduced in sim | 0.10 |
| 78 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 79 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 80 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 81 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 82 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 83 | agree-cex | cex at step 6 reproduced in sim | 0.08 |
| 84 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 85 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 86 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 87 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 88 | agree-cex | cex at step 11 reproduced in sim | 0.09 |
| 89 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 90 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 91 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 92 | agree-cex | cex at step 8 reproduced in sim | 0.08 |
| 93 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 94 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 95 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 96 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 97 | agree-cex | cex at step 8 reproduced in sim | 0.10 |
| 98 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 99 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 100 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 101 | agree-cex | cex at step 11 reproduced in sim | 0.10 |
| 102 | agree-cex | cex at step 11 reproduced in sim | 0.11 |
| 103 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 104 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 105 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 106 | agree-cex | cex at step 7 reproduced in sim | 0.08 |
| 107 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 108 | agree-cex | cex at step 11 reproduced in sim | 0.09 |
| 109 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 110 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 111 | agree-cex | cex at step 4 reproduced in sim | 0.10 |
| 112 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 113 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 114 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 115 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 116 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 117 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 118 | agree-cex | cex at step 6 reproduced in sim | 0.08 |
| 119 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 120 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 121 | agree-cex | cex at step 9 reproduced in sim | 0.09 |
| 122 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 123 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 124 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 125 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 126 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 127 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 128 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 129 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 130 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 131 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 132 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 133 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 134 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 135 | agree-cex | cex at step 0 reproduced in sim | 0.07 |
| 136 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 137 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 138 | agree-cex | cex at step 10 reproduced in sim | 0.09 |
| 139 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 140 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 141 | agree-cex | cex at step 5 reproduced in sim | 0.11 |
| 142 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 143 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 144 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 145 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 146 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 147 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 148 | agree-cex | cex at step 9 reproduced in sim | 0.09 |
| 149 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 150 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 151 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 152 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 153 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 154 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 155 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 156 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 157 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 158 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 159 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 160 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 161 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 162 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 163 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 164 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 165 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 166 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 167 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 168 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 169 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 170 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 171 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 172 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 173 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 174 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 175 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 176 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 177 | agree-cex | cex at step 8 reproduced in sim | 0.08 |
| 178 | agree-cex | cex at step 9 reproduced in sim | 0.10 |
| 179 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 180 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 181 | agree-cex | cex at step 9 reproduced in sim | 0.09 |
| 182 | agree-cex | cex at step 10 reproduced in sim | 0.09 |
| 183 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 184 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 185 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 186 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 187 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 188 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 189 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 190 | agree-cex | cex at step 9 reproduced in sim | 0.08 |
| 191 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 192 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 193 | agree-cex | cex at step 9 reproduced in sim | 0.10 |
| 194 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 195 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 196 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 197 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 198 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 199 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 200 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 201 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 202 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 203 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 204 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 205 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 206 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 207 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 208 | agree-cex | cex at step 10 reproduced in sim | 0.08 |
| 209 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 210 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 211 | agree-cex | cex at step 9 reproduced in sim | 0.10 |
| 212 | agree-cex | cex at step 7 reproduced in sim | 0.08 |
| 213 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 214 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 215 | agree-quiet | both legs quiet to bound 12 | 0.16 |
| 216 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 217 | agree-cex | cex at step 0 reproduced in sim | 0.07 |
| 218 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 219 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 220 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 221 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 222 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 223 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 224 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 225 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 226 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 227 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 228 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 229 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 230 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 231 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 232 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 233 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 234 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 235 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 236 | agree-cex | cex at step 10 reproduced in sim | 0.08 |
| 237 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 238 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 239 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 240 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 241 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 242 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 243 | agree-cex | cex at step 7 reproduced in sim | 0.10 |
| 244 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 245 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 246 | agree-cex | cex at step 11 reproduced in sim | 0.11 |
| 247 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 248 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 249 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 250 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 251 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 252 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 253 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 254 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 255 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 256 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 257 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 258 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 259 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 260 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 261 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 262 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 263 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 264 | agree-cex | cex at step 10 reproduced in sim | 0.09 |
| 265 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 266 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 267 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 268 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 269 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 270 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 271 | agree-cex | cex at step 6 reproduced in sim | 0.08 |
| 272 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 273 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 274 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 275 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 276 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 277 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 278 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 279 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 280 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 281 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 282 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 283 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 284 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 285 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 286 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 287 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 288 | agree-cex | cex at step 9 reproduced in sim | 0.08 |
| 289 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 290 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 291 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 292 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 293 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 294 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 295 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 296 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 297 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 298 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 299 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 300 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 301 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 302 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 303 | agree-cex | cex at step 9 reproduced in sim | 0.09 |
| 304 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 305 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 306 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 307 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 308 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 309 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 310 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 311 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 312 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 313 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 314 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 315 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 316 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 317 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 318 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 319 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 320 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 321 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 322 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 323 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 324 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 325 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 326 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 327 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 328 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 329 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 330 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 331 | agree-cex | cex at step 9 reproduced in sim | 0.09 |
| 332 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 333 | agree-cex | cex at step 6 reproduced in sim | 0.08 |
| 334 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 335 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 336 | agree-cex | cex at step 8 reproduced in sim | 0.08 |
| 337 | agree-cex | cex at step 9 reproduced in sim | 0.09 |
| 338 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 339 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 340 | agree-cex | cex at step 9 reproduced in sim | 0.09 |
| 341 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 342 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 343 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 344 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 345 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 346 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 347 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 348 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 349 | agree-cex | cex at step 7 reproduced in sim | 0.10 |
| 350 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 351 | agree-cex | cex at step 7 reproduced in sim | 0.10 |
| 352 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 353 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 354 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 355 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 356 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 357 | agree-cex | cex at step 6 reproduced in sim | 0.08 |
| 358 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 359 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 360 | agree-cex | cex at step 8 reproduced in sim | 0.08 |
| 361 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 362 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 363 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 364 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 365 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 366 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 367 | agree-cex | cex at step 9 reproduced in sim | 0.10 |
| 368 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 369 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 370 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 371 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 372 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 373 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 374 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 375 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 376 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 377 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 378 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 379 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 380 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 381 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 382 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 383 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 384 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 385 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 386 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 387 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 388 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 389 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 390 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 391 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 392 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 393 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 394 | agree-cex | cex at step 7 reproduced in sim | 0.08 |
| 395 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 396 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 397 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 398 | agree-cex | cex at step 3 reproduced in sim | 0.10 |
| 399 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 400 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 401 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 402 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 403 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 404 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 405 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 406 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 407 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 408 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 409 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 410 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 411 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 412 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 413 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 414 | agree-cex | cex at step 9 reproduced in sim | 0.08 |
| 415 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 416 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 417 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 418 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 419 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 420 | agree-cex | cex at step 1 reproduced in sim | 0.10 |
| 421 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 422 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 423 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 424 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 425 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 426 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 427 | agree-cex | cex at step 10 reproduced in sim | 0.10 |
| 428 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 429 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 430 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 431 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 432 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 433 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 434 | agree-cex | cex at step 10 reproduced in sim | 0.10 |
| 435 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 436 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 437 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 438 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 439 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 440 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 441 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 442 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 443 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 444 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 445 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 446 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 447 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 448 | agree-cex | cex at step 1 reproduced in sim | 0.10 |
| 449 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 450 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 451 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 452 | agree-cex | cex at step 6 reproduced in sim | 0.08 |
| 453 | agree-cex | cex at step 6 reproduced in sim | 0.08 |
| 454 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 455 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 456 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 457 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 458 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 459 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 460 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 461 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 462 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 463 | agree-cex | cex at step 6 reproduced in sim | 0.13 |
| 464 | agree-cex | cex at step 1 reproduced in sim | 0.13 |
| 465 | agree-cex | cex at step 7 reproduced in sim | 0.13 |
| 466 | agree-cex | cex at step 2 reproduced in sim | 0.12 |
| 467 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 468 | agree-cex | cex at step 3 reproduced in sim | 0.10 |
| 469 | agree-cex | cex at step 10 reproduced in sim | 0.14 |
| 470 | agree-cex | cex at step 3 reproduced in sim | 0.13 |
| 471 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 472 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 473 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 474 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 475 | agree-cex | cex at step 4 reproduced in sim | 0.10 |
| 476 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 477 | agree-cex | cex at step 8 reproduced in sim | 0.11 |
| 478 | agree-cex | cex at step 10 reproduced in sim | 0.09 |
| 479 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 480 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 481 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 482 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 483 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 484 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 485 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 486 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 487 | agree-cex | cex at step 5 reproduced in sim | 0.10 |
| 488 | agree-cex | cex at step 7 reproduced in sim | 0.10 |
| 489 | agree-cex | cex at step 5 reproduced in sim | 0.10 |
| 490 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 491 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 492 | agree-cex | cex at step 2 reproduced in sim | 0.10 |
| 493 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 494 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 495 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 496 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 497 | agree-cex | cex at step 7 reproduced in sim | 0.10 |
| 498 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 499 | agree-cex | cex at step 10 reproduced in sim | 0.11 |
| 500 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 501 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 502 | agree-cex | cex at step 8 reproduced in sim | 0.08 |
| 503 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 504 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 505 | agree-cex | cex at step 10 reproduced in sim | 0.10 |
| 506 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 507 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 508 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 509 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 510 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 511 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 512 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 513 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 514 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 515 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 516 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 517 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 518 | agree-cex | cex at step 7 reproduced in sim | 0.08 |
| 519 | agree-cex | cex at step 9 reproduced in sim | 0.08 |
| 520 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 521 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 522 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 523 | agree-cex | cex at step 11 reproduced in sim | 0.09 |
| 524 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 525 | agree-cex | cex at step 8 reproduced in sim | 0.08 |
| 526 | agree-cex | cex at step 9 reproduced in sim | 0.10 |
| 527 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 528 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 529 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 530 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 531 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 532 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 533 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 534 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 535 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 536 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 537 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 538 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 539 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 540 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 541 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 542 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 543 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 544 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 545 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 546 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 547 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 548 | agree-cex | cex at step 7 reproduced in sim | 0.08 |
| 549 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 550 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 551 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 552 | agree-cex | cex at step 7 reproduced in sim | 0.08 |
| 553 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 554 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 555 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 556 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 557 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 558 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 559 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 560 | agree-cex | cex at step 6 reproduced in sim | 0.11 |
| 561 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 562 | agree-cex | cex at step 9 reproduced in sim | 0.10 |
| 563 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 564 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 565 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 566 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 567 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 568 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 569 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 570 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 571 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 572 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 573 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 574 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 575 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 576 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 577 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 578 | agree-cex | cex at step 9 reproduced in sim | 0.10 |
| 579 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 580 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 581 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 582 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 583 | agree-cex | cex at step 11 reproduced in sim | 0.11 |
| 584 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 585 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 586 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 587 | agree-cex | cex at step 11 reproduced in sim | 0.09 |
| 588 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 589 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 590 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 591 | agree-cex | cex at step 8 reproduced in sim | 0.10 |
| 592 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 593 | agree-cex | cex at step 6 reproduced in sim | 0.08 |
| 594 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 595 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 596 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 597 | agree-cex | cex at step 7 reproduced in sim | 0.08 |
| 598 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 599 | agree-quiet | both legs quiet to bound 12 | 0.07 |
