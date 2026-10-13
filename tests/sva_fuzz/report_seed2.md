# sva_fuzz report -- seed 2, 600 cases, bound 12

- yosys: `/Users/npe/src/sv-yosys/third_party/yosys/build/yosys`
- smtbmc: `/Users/npe/src/sv-yosys/third_party/yosys/build/yosys-smtbmc`, solver z3
- wall time: 7.4 s (0.01 s/case)

## Totals

- cases: 600
- agreement: 600 (quiet 133, cex 467)
- bug-class findings: 0
- determinism guard: PASS (all 600 cases ran the sim leg twice with identical summaries)

## Class C signatures (frontend/lowering gaps, deduped)

- none

## Full per-case log

| index | class | detail | wall s |
|---|---|---|---|
| 0 | agree-quiet | both legs quiet to bound 12 | 0.16 |
| 1 | agree-cex | cex at step 8 reproduced in sim | 0.16 |
| 2 | agree-cex | cex at step 10 reproduced in sim | 0.15 |
| 3 | agree-quiet | both legs quiet to bound 12 | 0.14 |
| 4 | agree-cex | cex at step 9 reproduced in sim | 0.16 |
| 5 | agree-cex | cex at step 0 reproduced in sim | 0.16 |
| 6 | agree-cex | cex at step 0 reproduced in sim | 0.14 |
| 7 | agree-cex | cex at step 0 reproduced in sim | 0.14 |
| 8 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 9 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 10 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 11 | agree-cex | cex at step 6 reproduced in sim | 0.11 |
| 12 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 13 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 14 | agree-cex | cex at step 1 reproduced in sim | 0.10 |
| 15 | agree-cex | cex at step 3 reproduced in sim | 0.10 |
| 16 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 17 | agree-cex | cex at step 8 reproduced in sim | 0.10 |
| 18 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 19 | agree-cex | cex at step 4 reproduced in sim | 0.12 |
| 20 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 21 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 22 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 23 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 24 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 25 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 26 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 27 | agree-cex | cex at step 10 reproduced in sim | 0.11 |
| 28 | agree-cex | cex at step 5 reproduced in sim | 0.10 |
| 29 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 30 | agree-cex | cex at step 6 reproduced in sim | 0.10 |
| 31 | agree-cex | cex at step 2 reproduced in sim | 0.10 |
| 32 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 33 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 34 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 35 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 36 | agree-cex | cex at step 7 reproduced in sim | 0.10 |
| 37 | agree-cex | cex at step 1 reproduced in sim | 0.10 |
| 38 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 39 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 40 | agree-cex | cex at step 4 reproduced in sim | 0.10 |
| 41 | agree-cex | cex at step 6 reproduced in sim | 0.10 |
| 42 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 43 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 44 | agree-cex | cex at step 5 reproduced in sim | 0.10 |
| 45 | agree-cex | cex at step 3 reproduced in sim | 0.10 |
| 46 | agree-cex | cex at step 11 reproduced in sim | 0.12 |
| 47 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 48 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 49 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 50 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 51 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 52 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 53 | agree-cex | cex at step 8 reproduced in sim | 0.11 |
| 54 | agree-cex | cex at step 3 reproduced in sim | 0.10 |
| 55 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 56 | agree-cex | cex at step 4 reproduced in sim | 0.10 |
| 57 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 58 | agree-cex | cex at step 1 reproduced in sim | 0.10 |
| 59 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 60 | agree-cex | cex at step 3 reproduced in sim | 0.10 |
| 61 | agree-cex | cex at step 7 reproduced in sim | 0.11 |
| 62 | agree-cex | cex at step 10 reproduced in sim | 0.11 |
| 63 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 64 | agree-cex | cex at step 3 reproduced in sim | 0.10 |
| 65 | agree-quiet | both legs quiet to bound 12 | 0.12 |
| 66 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 67 | agree-cex | cex at step 3 reproduced in sim | 0.10 |
| 68 | agree-quiet | both legs quiet to bound 12 | 0.12 |
| 69 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 70 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 71 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 72 | agree-cex | cex at step 0 reproduced in sim | 0.11 |
| 73 | agree-quiet | both legs quiet to bound 12 | 0.12 |
| 74 | agree-cex | cex at step 4 reproduced in sim | 0.10 |
| 75 | agree-cex | cex at step 8 reproduced in sim | 0.10 |
| 76 | agree-cex | cex at step 2 reproduced in sim | 0.10 |
| 77 | agree-cex | cex at step 5 reproduced in sim | 0.10 |
| 78 | agree-cex | cex at step 0 reproduced in sim | 0.11 |
| 79 | agree-cex | cex at step 8 reproduced in sim | 0.10 |
| 80 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 81 | agree-cex | cex at step 1 reproduced in sim | 0.10 |
| 82 | agree-quiet | both legs quiet to bound 12 | 0.13 |
| 83 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 84 | agree-cex | cex at step 4 reproduced in sim | 0.10 |
| 85 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 86 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 87 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 88 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 89 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 90 | agree-cex | cex at step 1 reproduced in sim | 0.10 |
| 91 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 92 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 93 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 94 | agree-quiet | both legs quiet to bound 12 | 0.12 |
| 95 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 96 | agree-cex | cex at step 11 reproduced in sim | 0.11 |
| 97 | agree-cex | cex at step 11 reproduced in sim | 0.11 |
| 98 | agree-cex | cex at step 3 reproduced in sim | 0.10 |
| 99 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 100 | agree-cex | cex at step 1 reproduced in sim | 0.10 |
| 101 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 102 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 103 | agree-cex | cex at step 11 reproduced in sim | 0.10 |
| 104 | agree-cex | cex at step 6 reproduced in sim | 0.10 |
| 105 | agree-cex | cex at step 2 reproduced in sim | 0.10 |
| 106 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 107 | agree-cex | cex at step 5 reproduced in sim | 0.10 |
| 108 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 109 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 110 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 111 | agree-cex | cex at step 6 reproduced in sim | 0.10 |
| 112 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 113 | agree-cex | cex at step 5 reproduced in sim | 0.11 |
| 114 | agree-cex | cex at step 9 reproduced in sim | 0.10 |
| 115 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 116 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 117 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 118 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 119 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 120 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 121 | agree-cex | cex at step 6 reproduced in sim | 0.10 |
| 122 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 123 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 124 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 125 | agree-cex | cex at step 5 reproduced in sim | 0.10 |
| 126 | agree-cex | cex at step 4 reproduced in sim | 0.10 |
| 127 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 128 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 129 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 130 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 131 | agree-cex | cex at step 10 reproduced in sim | 0.10 |
| 132 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 133 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 134 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 135 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 136 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 137 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 138 | agree-cex | cex at step 11 reproduced in sim | 0.10 |
| 139 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 140 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 141 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 142 | agree-cex | cex at step 9 reproduced in sim | 0.12 |
| 143 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 144 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 145 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 146 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 147 | agree-cex | cex at step 2 reproduced in sim | 0.10 |
| 148 | agree-cex | cex at step 6 reproduced in sim | 0.10 |
| 149 | agree-cex | cex at step 6 reproduced in sim | 0.12 |
| 150 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 151 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 152 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 153 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 154 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 155 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 156 | agree-cex | cex at step 11 reproduced in sim | 0.10 |
| 157 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 158 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 159 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 160 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 161 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 162 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 163 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 164 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 165 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 166 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 167 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 168 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 169 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 170 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 171 | agree-cex | cex at step 10 reproduced in sim | 0.11 |
| 172 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 173 | agree-cex | cex at step 7 reproduced in sim | 0.10 |
| 174 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 175 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 176 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 177 | agree-cex | cex at step 5 reproduced in sim | 0.10 |
| 178 | agree-cex | cex at step 3 reproduced in sim | 0.10 |
| 179 | agree-cex | cex at step 7 reproduced in sim | 0.10 |
| 180 | agree-cex | cex at step 10 reproduced in sim | 0.10 |
| 181 | agree-cex | cex at step 10 reproduced in sim | 0.11 |
| 182 | agree-cex | cex at step 9 reproduced in sim | 0.10 |
| 183 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 184 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 185 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 186 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 187 | agree-cex | cex at step 11 reproduced in sim | 0.11 |
| 188 | agree-cex | cex at step 8 reproduced in sim | 0.11 |
| 189 | agree-cex | cex at step 8 reproduced in sim | 0.12 |
| 190 | agree-cex | cex at step 10 reproduced in sim | 0.12 |
| 191 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 192 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 193 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 194 | agree-cex | cex at step 10 reproduced in sim | 0.12 |
| 195 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 196 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 197 | agree-cex | cex at step 1 reproduced in sim | 0.11 |
| 198 | agree-cex | cex at step 7 reproduced in sim | 0.10 |
| 199 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 200 | agree-cex | cex at step 0 reproduced in sim | 0.11 |
| 201 | agree-quiet | both legs quiet to bound 12 | 0.12 |
| 202 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 203 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 204 | agree-cex | cex at step 8 reproduced in sim | 0.10 |
| 205 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 206 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 207 | agree-cex | cex at step 5 reproduced in sim | 0.10 |
| 208 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 209 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 210 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 211 | agree-cex | cex at step 4 reproduced in sim | 0.10 |
| 212 | agree-cex | cex at step 10 reproduced in sim | 0.11 |
| 213 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 214 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 215 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 216 | agree-cex | cex at step 3 reproduced in sim | 0.10 |
| 217 | agree-cex | cex at step 9 reproduced in sim | 0.09 |
| 218 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 219 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 220 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 221 | agree-cex | cex at step 5 reproduced in sim | 0.10 |
| 222 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 223 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 224 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 225 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 226 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 227 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 228 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 229 | agree-cex | cex at step 5 reproduced in sim | 0.10 |
| 230 | agree-cex | cex at step 7 reproduced in sim | 0.10 |
| 231 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 232 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 233 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 234 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 235 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 236 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 237 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 238 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 239 | agree-cex | cex at step 2 reproduced in sim | 0.10 |
| 240 | agree-cex | cex at step 10 reproduced in sim | 0.12 |
| 241 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 242 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 243 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 244 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 245 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 246 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 247 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 248 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 249 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 250 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 251 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 252 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 253 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 254 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 255 | agree-cex | cex at step 10 reproduced in sim | 0.12 |
| 256 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 257 | agree-cex | cex at step 2 reproduced in sim | 0.10 |
| 258 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 259 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 260 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 261 | agree-cex | cex at step 9 reproduced in sim | 0.09 |
| 262 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 263 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 264 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 265 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 266 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 267 | agree-cex | cex at step 8 reproduced in sim | 0.11 |
| 268 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 269 | agree-cex | cex at step 6 reproduced in sim | 0.10 |
| 270 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 271 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 272 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 273 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 274 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 275 | agree-cex | cex at step 6 reproduced in sim | 0.10 |
| 276 | agree-cex | cex at step 6 reproduced in sim | 0.10 |
| 277 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 278 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 279 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 280 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 281 | agree-cex | cex at step 8 reproduced in sim | 0.10 |
| 282 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 283 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 284 | agree-cex | cex at step 7 reproduced in sim | 0.10 |
| 285 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 286 | agree-cex | cex at step 5 reproduced in sim | 0.10 |
| 287 | agree-cex | cex at step 10 reproduced in sim | 0.10 |
| 288 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 289 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 290 | agree-cex | cex at step 6 reproduced in sim | 0.13 |
| 291 | agree-cex | cex at step 6 reproduced in sim | 0.11 |
| 292 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 293 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 294 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 295 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 296 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 297 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 298 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 299 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 300 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 301 | agree-cex | cex at step 4 reproduced in sim | 0.10 |
| 302 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 303 | agree-cex | cex at step 9 reproduced in sim | 0.09 |
| 304 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 305 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 306 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 307 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 308 | agree-cex | cex at step 4 reproduced in sim | 0.10 |
| 309 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 310 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 311 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 312 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 313 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 314 | agree-cex | cex at step 7 reproduced in sim | 0.10 |
| 315 | agree-cex | cex at step 8 reproduced in sim | 0.10 |
| 316 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 317 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 318 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 319 | agree-cex | cex at step 10 reproduced in sim | 0.11 |
| 320 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 321 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 322 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 323 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 324 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 325 | agree-cex | cex at step 7 reproduced in sim | 0.10 |
| 326 | agree-cex | cex at step 8 reproduced in sim | 0.10 |
| 327 | agree-cex | cex at step 6 reproduced in sim | 0.11 |
| 328 | agree-cex | cex at step 3 reproduced in sim | 0.10 |
| 329 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 330 | agree-cex | cex at step 1 reproduced in sim | 0.10 |
| 331 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 332 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 333 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 334 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 335 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 336 | agree-cex | cex at step 6 reproduced in sim | 0.10 |
| 337 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 338 | agree-cex | cex at step 8 reproduced in sim | 0.10 |
| 339 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 340 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 341 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 342 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 343 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 344 | agree-cex | cex at step 11 reproduced in sim | 0.10 |
| 345 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 346 | agree-cex | cex at step 10 reproduced in sim | 0.10 |
| 347 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 348 | agree-cex | cex at step 9 reproduced in sim | 0.10 |
| 349 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 350 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 351 | agree-cex | cex at step 9 reproduced in sim | 0.10 |
| 352 | agree-cex | cex at step 5 reproduced in sim | 0.10 |
| 353 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 354 | agree-cex | cex at step 7 reproduced in sim | 0.10 |
| 355 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 356 | agree-cex | cex at step 3 reproduced in sim | 0.10 |
| 357 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 358 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 359 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 360 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 361 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 362 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 363 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 364 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 365 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 366 | agree-cex | cex at step 11 reproduced in sim | 0.10 |
| 367 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 368 | agree-cex | cex at step 7 reproduced in sim | 0.10 |
| 369 | agree-cex | cex at step 7 reproduced in sim | 0.11 |
| 370 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 371 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 372 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 373 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 374 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 375 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 376 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 377 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 378 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 379 | agree-cex | cex at step 1 reproduced in sim | 0.10 |
| 380 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 381 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 382 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 383 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 384 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 385 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 386 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 387 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 388 | agree-cex | cex at step 6 reproduced in sim | 0.10 |
| 389 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 390 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 391 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 392 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 393 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 394 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 395 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 396 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 397 | agree-cex | cex at step 10 reproduced in sim | 0.11 |
| 398 | agree-cex | cex at step 8 reproduced in sim | 0.10 |
| 399 | agree-cex | cex at step 7 reproduced in sim | 0.10 |
| 400 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 401 | agree-cex | cex at step 7 reproduced in sim | 0.10 |
| 402 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 403 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 404 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 405 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 406 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 407 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 408 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 409 | agree-cex | cex at step 7 reproduced in sim | 0.10 |
| 410 | agree-cex | cex at step 6 reproduced in sim | 0.10 |
| 411 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 412 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 413 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 414 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 415 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 416 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 417 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 418 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 419 | agree-cex | cex at step 11 reproduced in sim | 0.13 |
| 420 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 421 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 422 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 423 | agree-cex | cex at step 11 reproduced in sim | 0.12 |
| 424 | agree-cex | cex at step 4 reproduced in sim | 0.10 |
| 425 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 426 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 427 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 428 | agree-cex | cex at step 6 reproduced in sim | 0.10 |
| 429 | agree-cex | cex at step 1 reproduced in sim | 0.10 |
| 430 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 431 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 432 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 433 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 434 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 435 | agree-cex | cex at step 7 reproduced in sim | 0.10 |
| 436 | agree-cex | cex at step 8 reproduced in sim | 0.10 |
| 437 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 438 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 439 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 440 | agree-cex | cex at step 11 reproduced in sim | 0.11 |
| 441 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 442 | agree-cex | cex at step 9 reproduced in sim | 0.10 |
| 443 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 444 | agree-cex | cex at step 7 reproduced in sim | 0.10 |
| 445 | agree-cex | cex at step 7 reproduced in sim | 0.11 |
| 446 | agree-cex | cex at step 8 reproduced in sim | 0.10 |
| 447 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 448 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 449 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 450 | agree-cex | cex at step 11 reproduced in sim | 0.24 |
| 451 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 452 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 453 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 454 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 455 | agree-cex | cex at step 9 reproduced in sim | 0.10 |
| 456 | agree-cex | cex at step 11 reproduced in sim | 0.11 |
| 457 | agree-cex | cex at step 11 reproduced in sim | 0.10 |
| 458 | agree-cex | cex at step 5 reproduced in sim | 0.10 |
| 459 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 460 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 461 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 462 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 463 | agree-cex | cex at step 5 reproduced in sim | 0.10 |
| 464 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 465 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 466 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 467 | agree-cex | cex at step 7 reproduced in sim | 0.10 |
| 468 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 469 | agree-cex | cex at step 2 reproduced in sim | 0.10 |
| 470 | agree-cex | cex at step 10 reproduced in sim | 0.09 |
| 471 | agree-cex | cex at step 11 reproduced in sim | 0.10 |
| 472 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 473 | agree-cex | cex at step 3 reproduced in sim | 0.10 |
| 474 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 475 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 476 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 477 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 478 | agree-cex | cex at step 5 reproduced in sim | 0.10 |
| 479 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 480 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 481 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 482 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 483 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 484 | agree-cex | cex at step 7 reproduced in sim | 0.10 |
| 485 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 486 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 487 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 488 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 489 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 490 | agree-cex | cex at step 7 reproduced in sim | 0.10 |
| 491 | agree-cex | cex at step 11 reproduced in sim | 0.11 |
| 492 | agree-cex | cex at step 11 reproduced in sim | 0.10 |
| 493 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 494 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 495 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 496 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 497 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 498 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 499 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 500 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 501 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 502 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 503 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 504 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 505 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 506 | agree-cex | cex at step 8 reproduced in sim | 0.10 |
| 507 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 508 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 509 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 510 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 511 | agree-cex | cex at step 7 reproduced in sim | 0.10 |
| 512 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 513 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 514 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 515 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 516 | agree-cex | cex at step 10 reproduced in sim | 0.11 |
| 517 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 518 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 519 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 520 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 521 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 522 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 523 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 524 | agree-cex | cex at step 6 reproduced in sim | 0.11 |
| 525 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 526 | agree-cex | cex at step 11 reproduced in sim | 0.09 |
| 527 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 528 | agree-cex | cex at step 9 reproduced in sim | 0.10 |
| 529 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 530 | agree-cex | cex at step 6 reproduced in sim | 0.10 |
| 531 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 532 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 533 | agree-cex | cex at step 4 reproduced in sim | 0.10 |
| 534 | agree-cex | cex at step 5 reproduced in sim | 0.12 |
| 535 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 536 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 537 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 538 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 539 | agree-cex | cex at step 6 reproduced in sim | 0.10 |
| 540 | agree-cex | cex at step 5 reproduced in sim | 0.10 |
| 541 | agree-cex | cex at step 6 reproduced in sim | 0.10 |
| 542 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 543 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 544 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 545 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 546 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 547 | agree-cex | cex at step 11 reproduced in sim | 0.11 |
| 548 | agree-cex | cex at step 10 reproduced in sim | 0.11 |
| 549 | agree-cex | cex at step 11 reproduced in sim | 0.10 |
| 550 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 551 | agree-cex | cex at step 4 reproduced in sim | 0.10 |
| 552 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 553 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 554 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 555 | agree-cex | cex at step 3 reproduced in sim | 0.10 |
| 556 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 557 | agree-cex | cex at step 3 reproduced in sim | 0.10 |
| 558 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 559 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 560 | agree-cex | cex at step 2 reproduced in sim | 0.10 |
| 561 | agree-quiet | both legs quiet to bound 12 | 0.13 |
| 562 | agree-cex | cex at step 4 reproduced in sim | 0.10 |
| 563 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 564 | agree-cex | cex at step 5 reproduced in sim | 0.11 |
| 565 | agree-cex | cex at step 1 reproduced in sim | 0.10 |
| 566 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 567 | agree-quiet | both legs quiet to bound 12 | 0.12 |
| 568 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 569 | agree-cex | cex at step 9 reproduced in sim | 0.11 |
| 570 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 571 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 572 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 573 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 574 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 575 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 576 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 577 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 578 | agree-cex | cex at step 8 reproduced in sim | 0.12 |
| 579 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 580 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 581 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 582 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 583 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 584 | agree-cex | cex at step 7 reproduced in sim | 0.10 |
| 585 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 586 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 587 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 588 | agree-cex | cex at step 7 reproduced in sim | 0.10 |
| 589 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 590 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 591 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 592 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 593 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 594 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 595 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 596 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 597 | agree-cex | cex at step 10 reproduced in sim | 0.09 |
| 598 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 599 | agree-quiet | both legs quiet to bound 12 | 0.07 |
