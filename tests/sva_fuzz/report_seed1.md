# sva_fuzz report -- seed 1, 600 cases, bound 12

- yosys: `/Users/npe/src/sv-yosys/third_party/yosys/build/yosys`
- smtbmc: `/Users/npe/src/sv-yosys/third_party/yosys/build/yosys-smtbmc`, solver z3
- wall time: 18.3 s (0.03 s/case)

## Totals

- cases: 600
- agreement: 600 (quiet 143, cex 457)
- bug-class findings: 0
- determinism guard: PASS (all 600 cases ran the sim leg twice with identical summaries)

## Class C signatures (frontend/lowering gaps, deduped)

- none

## Full per-case log

| index | class | detail | wall s |
|---|---|---|---|
| 0 | agree-quiet | both legs quiet to bound 12 | 0.22 |
| 1 | agree-cex | cex at step 2 reproduced in sim | 0.23 |
| 2 | agree-cex | cex at step 1 reproduced in sim | 0.24 |
| 3 | agree-quiet | both legs quiet to bound 12 | 0.22 |
| 4 | agree-cex | cex at step 0 reproduced in sim | 0.25 |
| 5 | agree-quiet | both legs quiet to bound 12 | 0.22 |
| 6 | agree-cex | cex at step 1 reproduced in sim | 0.24 |
| 7 | agree-cex | cex at step 0 reproduced in sim | 0.23 |
| 8 | agree-cex | cex at step 4 reproduced in sim | 0.17 |
| 9 | agree-cex | cex at step 2 reproduced in sim | 0.16 |
| 10 | agree-cex | cex at step 0 reproduced in sim | 0.24 |
| 11 | agree-cex | cex at step 0 reproduced in sim | 0.23 |
| 12 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 13 | agree-cex | cex at step 4 reproduced in sim | 0.21 |
| 14 | agree-cex | cex at step 7 reproduced in sim | 0.24 |
| 15 | agree-cex | cex at step 1 reproduced in sim | 0.16 |
| 16 | agree-cex | cex at step 0 reproduced in sim | 0.23 |
| 17 | agree-quiet | both legs quiet to bound 12 | 0.26 |
| 18 | agree-cex | cex at step 3 reproduced in sim | 0.30 |
| 19 | agree-quiet | both legs quiet to bound 12 | 0.32 |
| 20 | agree-cex | cex at step 6 reproduced in sim | 0.19 |
| 21 | agree-cex | cex at step 8 reproduced in sim | 0.25 |
| 22 | agree-cex | cex at step 5 reproduced in sim | 0.26 |
| 23 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 24 | agree-cex | cex at step 7 reproduced in sim | 0.18 |
| 25 | agree-quiet | both legs quiet to bound 12 | 0.16 |
| 26 | agree-cex | cex at step 0 reproduced in sim | 0.18 |
| 27 | agree-cex | cex at step 6 reproduced in sim | 0.21 |
| 28 | agree-cex | cex at step 0 reproduced in sim | 0.28 |
| 29 | agree-cex | cex at step 6 reproduced in sim | 0.30 |
| 30 | agree-quiet | both legs quiet to bound 12 | 0.25 |
| 31 | agree-cex | cex at step 3 reproduced in sim | 0.22 |
| 32 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 33 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 34 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 35 | agree-cex | cex at step 6 reproduced in sim | 0.23 |
| 36 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 37 | agree-cex | cex at step 2 reproduced in sim | 0.23 |
| 38 | agree-cex | cex at step 6 reproduced in sim | 0.24 |
| 39 | agree-quiet | both legs quiet to bound 12 | 0.24 |
| 40 | agree-cex | cex at step 1 reproduced in sim | 0.23 |
| 41 | agree-cex | cex at step 5 reproduced in sim | 0.24 |
| 42 | agree-cex | cex at step 0 reproduced in sim | 0.18 |
| 43 | agree-cex | cex at step 3 reproduced in sim | 0.18 |
| 44 | agree-cex | cex at step 5 reproduced in sim | 0.17 |
| 45 | agree-cex | cex at step 2 reproduced in sim | 0.27 |
| 46 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 47 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 48 | agree-cex | cex at step 0 reproduced in sim | 0.28 |
| 49 | agree-cex | cex at step 1 reproduced in sim | 0.27 |
| 50 | agree-quiet | both legs quiet to bound 12 | 0.29 |
| 51 | agree-cex | cex at step 1 reproduced in sim | 0.25 |
| 52 | agree-cex | cex at step 2 reproduced in sim | 0.22 |
| 53 | agree-cex | cex at step 6 reproduced in sim | 0.24 |
| 54 | agree-cex | cex at step 3 reproduced in sim | 0.25 |
| 55 | agree-cex | cex at step 0 reproduced in sim | 0.25 |
| 56 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 57 | agree-cex | cex at step 4 reproduced in sim | 0.29 |
| 58 | agree-cex | cex at step 4 reproduced in sim | 0.28 |
| 59 | agree-cex | cex at step 6 reproduced in sim | 0.20 |
| 60 | agree-quiet | both legs quiet to bound 12 | 0.17 |
| 61 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 62 | agree-quiet | both legs quiet to bound 12 | 0.28 |
| 63 | agree-quiet | both legs quiet to bound 12 | 0.20 |
| 64 | agree-cex | cex at step 1 reproduced in sim | 0.21 |
| 65 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 66 | agree-quiet | both legs quiet to bound 12 | 0.26 |
| 67 | agree-cex | cex at step 10 reproduced in sim | 0.26 |
| 68 | agree-cex | cex at step 4 reproduced in sim | 0.22 |
| 69 | agree-cex | cex at step 4 reproduced in sim | 0.28 |
| 70 | agree-cex | cex at step 7 reproduced in sim | 0.22 |
| 71 | agree-cex | cex at step 11 reproduced in sim | 0.28 |
| 72 | agree-cex | cex at step 3 reproduced in sim | 0.25 |
| 73 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 74 | agree-quiet | both legs quiet to bound 12 | 0.29 |
| 75 | agree-quiet | both legs quiet to bound 12 | 0.27 |
| 76 | agree-cex | cex at step 4 reproduced in sim | 0.31 |
| 77 | agree-quiet | both legs quiet to bound 12 | 0.26 |
| 78 | agree-cex | cex at step 7 reproduced in sim | 0.28 |
| 79 | agree-cex | cex at step 0 reproduced in sim | 0.23 |
| 80 | agree-cex | cex at step 9 reproduced in sim | 0.27 |
| 81 | agree-quiet | both legs quiet to bound 12 | 0.27 |
| 82 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 83 | agree-cex | cex at step 5 reproduced in sim | 0.22 |
| 84 | agree-cex | cex at step 6 reproduced in sim | 0.23 |
| 85 | agree-quiet | both legs quiet to bound 12 | 0.25 |
| 86 | agree-cex | cex at step 8 reproduced in sim | 0.27 |
| 87 | agree-cex | cex at step 2 reproduced in sim | 0.21 |
| 88 | agree-cex | cex at step 9 reproduced in sim | 0.31 |
| 89 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 90 | agree-cex | cex at step 9 reproduced in sim | 0.28 |
| 91 | agree-cex | cex at step 0 reproduced in sim | 0.25 |
| 92 | agree-cex | cex at step 2 reproduced in sim | 0.21 |
| 93 | agree-cex | cex at step 4 reproduced in sim | 0.24 |
| 94 | agree-cex | cex at step 5 reproduced in sim | 0.24 |
| 95 | agree-cex | cex at step 7 reproduced in sim | 0.25 |
| 96 | agree-cex | cex at step 1 reproduced in sim | 0.23 |
| 97 | agree-cex | cex at step 6 reproduced in sim | 0.29 |
| 98 | agree-cex | cex at step 0 reproduced in sim | 0.27 |
| 99 | agree-cex | cex at step 6 reproduced in sim | 0.27 |
| 100 | agree-cex | cex at step 0 reproduced in sim | 0.24 |
| 101 | agree-cex | cex at step 8 reproduced in sim | 0.31 |
| 102 | agree-cex | cex at step 4 reproduced in sim | 0.21 |
| 103 | agree-cex | cex at step 4 reproduced in sim | 0.23 |
| 104 | agree-cex | cex at step 9 reproduced in sim | 0.24 |
| 105 | agree-quiet | both legs quiet to bound 12 | 0.20 |
| 106 | agree-cex | cex at step 1 reproduced in sim | 0.21 |
| 107 | agree-cex | cex at step 3 reproduced in sim | 0.21 |
| 108 | agree-quiet | both legs quiet to bound 12 | 0.20 |
| 109 | agree-cex | cex at step 10 reproduced in sim | 0.23 |
| 110 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 111 | agree-cex | cex at step 1 reproduced in sim | 0.23 |
| 112 | agree-cex | cex at step 2 reproduced in sim | 0.26 |
| 113 | agree-cex | cex at step 2 reproduced in sim | 0.29 |
| 114 | agree-quiet | both legs quiet to bound 12 | 0.20 |
| 115 | agree-cex | cex at step 0 reproduced in sim | 0.27 |
| 116 | agree-cex | cex at step 6 reproduced in sim | 0.22 |
| 117 | agree-cex | cex at step 0 reproduced in sim | 0.23 |
| 118 | agree-cex | cex at step 2 reproduced in sim | 0.21 |
| 119 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 120 | agree-cex | cex at step 3 reproduced in sim | 0.21 |
| 121 | agree-cex | cex at step 3 reproduced in sim | 0.28 |
| 122 | agree-cex | cex at step 1 reproduced in sim | 0.27 |
| 123 | agree-cex | cex at step 1 reproduced in sim | 0.23 |
| 124 | agree-cex | cex at step 1 reproduced in sim | 0.23 |
| 125 | agree-cex | cex at step 11 reproduced in sim | 0.28 |
| 126 | agree-cex | cex at step 2 reproduced in sim | 0.27 |
| 127 | agree-cex | cex at step 8 reproduced in sim | 0.32 |
| 128 | agree-cex | cex at step 7 reproduced in sim | 0.27 |
| 129 | agree-cex | cex at step 5 reproduced in sim | 0.28 |
| 130 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 131 | agree-cex | cex at step 3 reproduced in sim | 0.20 |
| 132 | agree-cex | cex at step 0 reproduced in sim | 0.26 |
| 133 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 134 | agree-cex | cex at step 4 reproduced in sim | 0.19 |
| 135 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 136 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 137 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 138 | agree-quiet | both legs quiet to bound 12 | 0.32 |
| 139 | agree-cex | cex at step 8 reproduced in sim | 0.30 |
| 140 | agree-quiet | both legs quiet to bound 12 | 0.26 |
| 141 | agree-cex | cex at step 1 reproduced in sim | 0.19 |
| 142 | agree-cex | cex at step 3 reproduced in sim | 0.22 |
| 143 | agree-quiet | both legs quiet to bound 12 | 0.22 |
| 144 | agree-cex | cex at step 8 reproduced in sim | 0.21 |
| 145 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 146 | agree-cex | cex at step 0 reproduced in sim | 0.23 |
| 147 | agree-cex | cex at step 9 reproduced in sim | 0.23 |
| 148 | agree-cex | cex at step 1 reproduced in sim | 0.22 |
| 149 | agree-cex | cex at step 8 reproduced in sim | 0.28 |
| 150 | agree-cex | cex at step 0 reproduced in sim | 0.18 |
| 151 | agree-cex | cex at step 7 reproduced in sim | 0.32 |
| 152 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 153 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 154 | agree-cex | cex at step 6 reproduced in sim | 0.23 |
| 155 | agree-cex | cex at step 5 reproduced in sim | 0.19 |
| 156 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 157 | agree-quiet | both legs quiet to bound 12 | 0.25 |
| 158 | agree-quiet | both legs quiet to bound 12 | 0.25 |
| 159 | agree-cex | cex at step 0 reproduced in sim | 0.24 |
| 160 | agree-cex | cex at step 1 reproduced in sim | 0.20 |
| 161 | agree-cex | cex at step 4 reproduced in sim | 0.21 |
| 162 | agree-quiet | both legs quiet to bound 12 | 0.27 |
| 163 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 164 | agree-cex | cex at step 1 reproduced in sim | 0.24 |
| 165 | agree-cex | cex at step 3 reproduced in sim | 0.26 |
| 166 | agree-cex | cex at step 4 reproduced in sim | 0.24 |
| 167 | agree-cex | cex at step 10 reproduced in sim | 0.63 |
| 168 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 169 | agree-cex | cex at step 9 reproduced in sim | 0.28 |
| 170 | agree-cex | cex at step 11 reproduced in sim | 0.39 |
| 171 | agree-cex | cex at step 3 reproduced in sim | 0.20 |
| 172 | agree-cex | cex at step 7 reproduced in sim | 0.30 |
| 173 | agree-quiet | both legs quiet to bound 12 | 0.24 |
| 174 | agree-cex | cex at step 5 reproduced in sim | 0.25 |
| 175 | agree-cex | cex at step 9 reproduced in sim | 0.25 |
| 176 | agree-quiet | both legs quiet to bound 12 | 0.14 |
| 177 | agree-cex | cex at step 2 reproduced in sim | 0.17 |
| 178 | agree-cex | cex at step 3 reproduced in sim | 0.26 |
| 179 | agree-quiet | both legs quiet to bound 12 | 0.24 |
| 180 | agree-cex | cex at step 0 reproduced in sim | 0.28 |
| 181 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 182 | agree-cex | cex at step 3 reproduced in sim | 0.31 |
| 183 | agree-cex | cex at step 3 reproduced in sim | 0.28 |
| 184 | agree-cex | cex at step 6 reproduced in sim | 0.20 |
| 185 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 186 | agree-cex | cex at step 8 reproduced in sim | 0.23 |
| 187 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 188 | agree-cex | cex at step 6 reproduced in sim | 0.23 |
| 189 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 190 | agree-cex | cex at step 3 reproduced in sim | 0.17 |
| 191 | agree-cex | cex at step 4 reproduced in sim | 0.18 |
| 192 | agree-quiet | both legs quiet to bound 12 | 0.22 |
| 193 | agree-cex | cex at step 0 reproduced in sim | 0.18 |
| 194 | agree-cex | cex at step 3 reproduced in sim | 0.21 |
| 195 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 196 | agree-cex | cex at step 6 reproduced in sim | 0.24 |
| 197 | agree-cex | cex at step 0 reproduced in sim | 0.23 |
| 198 | agree-cex | cex at step 1 reproduced in sim | 0.17 |
| 199 | agree-quiet | both legs quiet to bound 12 | 0.24 |
| 200 | agree-cex | cex at step 3 reproduced in sim | 0.23 |
| 201 | agree-cex | cex at step 0 reproduced in sim | 0.26 |
| 202 | agree-quiet | both legs quiet to bound 12 | 0.57 |
| 203 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 204 | agree-cex | cex at step 0 reproduced in sim | 0.32 |
| 205 | agree-cex | cex at step 3 reproduced in sim | 0.26 |
| 206 | agree-cex | cex at step 0 reproduced in sim | 0.23 |
| 207 | agree-cex | cex at step 0 reproduced in sim | 0.25 |
| 208 | agree-cex | cex at step 3 reproduced in sim | 0.29 |
| 209 | agree-cex | cex at step 2 reproduced in sim | 0.23 |
| 210 | agree-quiet | both legs quiet to bound 12 | 0.24 |
| 211 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 212 | agree-cex | cex at step 8 reproduced in sim | 0.26 |
| 213 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 214 | agree-cex | cex at step 3 reproduced in sim | 0.17 |
| 215 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 216 | agree-cex | cex at step 5 reproduced in sim | 0.23 |
| 217 | agree-cex | cex at step 8 reproduced in sim | 0.27 |
| 218 | agree-cex | cex at step 1 reproduced in sim | 0.18 |
| 219 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 220 | agree-quiet | both legs quiet to bound 12 | 0.27 |
| 221 | agree-cex | cex at step 10 reproduced in sim | 0.23 |
| 222 | agree-quiet | both legs quiet to bound 12 | 0.33 |
| 223 | agree-quiet | both legs quiet to bound 12 | 0.22 |
| 224 | agree-quiet | both legs quiet to bound 12 | 0.22 |
| 225 | agree-cex | cex at step 7 reproduced in sim | 0.29 |
| 226 | agree-cex | cex at step 10 reproduced in sim | 0.22 |
| 227 | agree-cex | cex at step 7 reproduced in sim | 0.20 |
| 228 | agree-quiet | both legs quiet to bound 12 | 0.13 |
| 229 | agree-cex | cex at step 9 reproduced in sim | 0.23 |
| 230 | agree-quiet | both legs quiet to bound 12 | 0.20 |
| 231 | agree-quiet | both legs quiet to bound 12 | 0.33 |
| 232 | agree-cex | cex at step 0 reproduced in sim | 0.27 |
| 233 | agree-cex | cex at step 0 reproduced in sim | 0.23 |
| 234 | agree-quiet | both legs quiet to bound 12 | 0.27 |
| 235 | agree-quiet | both legs quiet to bound 12 | 0.25 |
| 236 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 237 | agree-cex | cex at step 11 reproduced in sim | 0.26 |
| 238 | agree-quiet | both legs quiet to bound 12 | 0.17 |
| 239 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 240 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 241 | agree-cex | cex at step 5 reproduced in sim | 0.21 |
| 242 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 243 | agree-cex | cex at step 2 reproduced in sim | 0.20 |
| 244 | agree-cex | cex at step 0 reproduced in sim | 0.18 |
| 245 | agree-cex | cex at step 3 reproduced in sim | 0.24 |
| 246 | agree-cex | cex at step 0 reproduced in sim | 0.17 |
| 247 | agree-cex | cex at step 4 reproduced in sim | 0.17 |
| 248 | agree-cex | cex at step 6 reproduced in sim | 0.19 |
| 249 | agree-cex | cex at step 4 reproduced in sim | 0.21 |
| 250 | agree-cex | cex at step 7 reproduced in sim | 0.18 |
| 251 | agree-cex | cex at step 3 reproduced in sim | 0.18 |
| 252 | agree-quiet | both legs quiet to bound 12 | 0.20 |
| 253 | agree-cex | cex at step 0 reproduced in sim | 0.15 |
| 254 | agree-quiet | both legs quiet to bound 12 | 0.17 |
| 255 | agree-cex | cex at step 0 reproduced in sim | 0.16 |
| 256 | agree-cex | cex at step 0 reproduced in sim | 0.16 |
| 257 | agree-cex | cex at step 6 reproduced in sim | 0.24 |
| 258 | agree-cex | cex at step 11 reproduced in sim | 0.30 |
| 259 | agree-quiet | both legs quiet to bound 12 | 0.27 |
| 260 | agree-quiet | both legs quiet to bound 12 | 0.24 |
| 261 | agree-cex | cex at step 2 reproduced in sim | 0.27 |
| 262 | agree-cex | cex at step 5 reproduced in sim | 0.20 |
| 263 | agree-cex | cex at step 1 reproduced in sim | 0.20 |
| 264 | agree-cex | cex at step 0 reproduced in sim | 0.24 |
| 265 | agree-cex | cex at step 2 reproduced in sim | 0.17 |
| 266 | agree-cex | cex at step 0 reproduced in sim | 0.16 |
| 267 | agree-cex | cex at step 1 reproduced in sim | 0.18 |
| 268 | agree-cex | cex at step 6 reproduced in sim | 0.20 |
| 269 | agree-cex | cex at step 10 reproduced in sim | 0.30 |
| 270 | agree-cex | cex at step 10 reproduced in sim | 0.22 |
| 271 | agree-cex | cex at step 5 reproduced in sim | 0.25 |
| 272 | agree-cex | cex at step 3 reproduced in sim | 0.21 |
| 273 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 274 | agree-cex | cex at step 0 reproduced in sim | 0.16 |
| 275 | agree-cex | cex at step 1 reproduced in sim | 0.18 |
| 276 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 277 | agree-quiet | both legs quiet to bound 12 | 0.22 |
| 278 | agree-cex | cex at step 10 reproduced in sim | 0.26 |
| 279 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 280 | agree-cex | cex at step 3 reproduced in sim | 0.19 |
| 281 | agree-cex | cex at step 11 reproduced in sim | 0.21 |
| 282 | agree-cex | cex at step 9 reproduced in sim | 0.30 |
| 283 | agree-cex | cex at step 6 reproduced in sim | 0.24 |
| 284 | agree-cex | cex at step 3 reproduced in sim | 0.21 |
| 285 | agree-cex | cex at step 7 reproduced in sim | 0.26 |
| 286 | agree-cex | cex at step 4 reproduced in sim | 0.25 |
| 287 | agree-quiet | both legs quiet to bound 12 | 0.22 |
| 288 | agree-cex | cex at step 1 reproduced in sim | 0.20 |
| 289 | agree-cex | cex at step 4 reproduced in sim | 0.23 |
| 290 | agree-cex | cex at step 5 reproduced in sim | 0.17 |
| 291 | agree-cex | cex at step 0 reproduced in sim | 0.23 |
| 292 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 293 | agree-cex | cex at step 10 reproduced in sim | 0.27 |
| 294 | agree-cex | cex at step 1 reproduced in sim | 0.21 |
| 295 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 296 | agree-quiet | both legs quiet to bound 12 | 0.26 |
| 297 | agree-cex | cex at step 0 reproduced in sim | 0.23 |
| 298 | agree-cex | cex at step 3 reproduced in sim | 0.27 |
| 299 | agree-cex | cex at step 4 reproduced in sim | 0.19 |
| 300 | agree-cex | cex at step 0 reproduced in sim | 0.16 |
| 301 | agree-quiet | both legs quiet to bound 12 | 0.16 |
| 302 | agree-cex | cex at step 4 reproduced in sim | 0.21 |
| 303 | agree-cex | cex at step 6 reproduced in sim | 0.29 |
| 304 | agree-cex | cex at step 6 reproduced in sim | 0.22 |
| 305 | agree-cex | cex at step 7 reproduced in sim | 0.25 |
| 306 | agree-cex | cex at step 7 reproduced in sim | 0.28 |
| 307 | agree-cex | cex at step 6 reproduced in sim | 0.22 |
| 308 | agree-quiet | both legs quiet to bound 12 | 0.29 |
| 309 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 310 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 311 | agree-cex | cex at step 1 reproduced in sim | 0.22 |
| 312 | agree-cex | cex at step 9 reproduced in sim | 0.23 |
| 313 | agree-quiet | both legs quiet to bound 12 | 0.24 |
| 314 | agree-cex | cex at step 5 reproduced in sim | 0.26 |
| 315 | agree-cex | cex at step 11 reproduced in sim | 0.29 |
| 316 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 317 | agree-cex | cex at step 2 reproduced in sim | 0.25 |
| 318 | agree-cex | cex at step 0 reproduced in sim | 0.24 |
| 319 | agree-cex | cex at step 11 reproduced in sim | 0.22 |
| 320 | agree-cex | cex at step 5 reproduced in sim | 0.22 |
| 321 | agree-cex | cex at step 0 reproduced in sim | 0.24 |
| 322 | agree-cex | cex at step 6 reproduced in sim | 0.25 |
| 323 | agree-cex | cex at step 0 reproduced in sim | 0.31 |
| 324 | agree-cex | cex at step 2 reproduced in sim | 0.20 |
| 325 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 326 | agree-cex | cex at step 0 reproduced in sim | 0.17 |
| 327 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 328 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 329 | agree-quiet | both legs quiet to bound 12 | 0.29 |
| 330 | agree-cex | cex at step 0 reproduced in sim | 0.27 |
| 331 | agree-cex | cex at step 7 reproduced in sim | 0.24 |
| 332 | agree-cex | cex at step 4 reproduced in sim | 0.16 |
| 333 | agree-cex | cex at step 3 reproduced in sim | 0.18 |
| 334 | agree-cex | cex at step 3 reproduced in sim | 0.21 |
| 335 | agree-cex | cex at step 1 reproduced in sim | 0.19 |
| 336 | agree-cex | cex at step 0 reproduced in sim | 0.23 |
| 337 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 338 | agree-quiet | both legs quiet to bound 12 | 0.24 |
| 339 | agree-cex | cex at step 9 reproduced in sim | 0.25 |
| 340 | agree-cex | cex at step 10 reproduced in sim | 0.28 |
| 341 | agree-cex | cex at step 7 reproduced in sim | 0.26 |
| 342 | agree-cex | cex at step 10 reproduced in sim | 0.18 |
| 343 | agree-quiet | both legs quiet to bound 12 | 0.16 |
| 344 | agree-cex | cex at step 0 reproduced in sim | 0.18 |
| 345 | agree-cex | cex at step 0 reproduced in sim | 0.17 |
| 346 | agree-cex | cex at step 0 reproduced in sim | 0.17 |
| 347 | agree-quiet | both legs quiet to bound 12 | 0.17 |
| 348 | agree-quiet | both legs quiet to bound 12 | 0.24 |
| 349 | agree-cex | cex at step 6 reproduced in sim | 0.30 |
| 350 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 351 | agree-cex | cex at step 6 reproduced in sim | 0.21 |
| 352 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 353 | agree-cex | cex at step 2 reproduced in sim | 0.19 |
| 354 | agree-cex | cex at step 6 reproduced in sim | 0.29 |
| 355 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 356 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 357 | agree-quiet | both legs quiet to bound 12 | 0.20 |
| 358 | agree-cex | cex at step 7 reproduced in sim | 0.22 |
| 359 | agree-quiet | both legs quiet to bound 12 | 0.26 |
| 360 | agree-quiet | both legs quiet to bound 12 | 0.25 |
| 361 | agree-cex | cex at step 2 reproduced in sim | 0.22 |
| 362 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 363 | agree-quiet | both legs quiet to bound 12 | 0.26 |
| 364 | agree-quiet | both legs quiet to bound 12 | 0.17 |
| 365 | agree-cex | cex at step 1 reproduced in sim | 0.23 |
| 366 | agree-cex | cex at step 9 reproduced in sim | 0.20 |
| 367 | agree-cex | cex at step 1 reproduced in sim | 0.25 |
| 368 | agree-cex | cex at step 0 reproduced in sim | 0.25 |
| 369 | agree-cex | cex at step 0 reproduced in sim | 0.18 |
| 370 | agree-quiet | both legs quiet to bound 12 | 0.25 |
| 371 | agree-cex | cex at step 3 reproduced in sim | 0.30 |
| 372 | agree-cex | cex at step 2 reproduced in sim | 0.30 |
| 373 | agree-cex | cex at step 10 reproduced in sim | 0.22 |
| 374 | agree-cex | cex at step 8 reproduced in sim | 0.25 |
| 375 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 376 | agree-cex | cex at step 0 reproduced in sim | 0.28 |
| 377 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 378 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 379 | agree-cex | cex at step 8 reproduced in sim | 0.20 |
| 380 | agree-quiet | both legs quiet to bound 12 | 0.27 |
| 381 | agree-cex | cex at step 5 reproduced in sim | 0.31 |
| 382 | agree-cex | cex at step 9 reproduced in sim | 0.23 |
| 383 | agree-cex | cex at step 1 reproduced in sim | 0.20 |
| 384 | agree-cex | cex at step 4 reproduced in sim | 0.23 |
| 385 | agree-quiet | both legs quiet to bound 12 | 0.24 |
| 386 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 387 | agree-quiet | both legs quiet to bound 12 | 0.17 |
| 388 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 389 | agree-cex | cex at step 11 reproduced in sim | 0.24 |
| 390 | agree-cex | cex at step 6 reproduced in sim | 0.27 |
| 391 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 392 | agree-cex | cex at step 4 reproduced in sim | 0.22 |
| 393 | agree-cex | cex at step 0 reproduced in sim | 0.25 |
| 394 | agree-cex | cex at step 0 reproduced in sim | 0.29 |
| 395 | agree-quiet | both legs quiet to bound 12 | 0.22 |
| 396 | agree-cex | cex at step 5 reproduced in sim | 0.24 |
| 397 | agree-cex | cex at step 2 reproduced in sim | 0.26 |
| 398 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 399 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 400 | agree-quiet | both legs quiet to bound 12 | 0.35 |
| 401 | agree-cex | cex at step 0 reproduced in sim | 0.25 |
| 402 | agree-cex | cex at step 5 reproduced in sim | 0.27 |
| 403 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 404 | agree-cex | cex at step 4 reproduced in sim | 0.26 |
| 405 | agree-quiet | both legs quiet to bound 12 | 0.26 |
| 406 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 407 | agree-cex | cex at step 0 reproduced in sim | 0.26 |
| 408 | agree-cex | cex at step 0 reproduced in sim | 0.25 |
| 409 | agree-cex | cex at step 0 reproduced in sim | 0.28 |
| 410 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 411 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 412 | agree-cex | cex at step 7 reproduced in sim | 0.26 |
| 413 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 414 | agree-cex | cex at step 5 reproduced in sim | 0.23 |
| 415 | agree-cex | cex at step 0 reproduced in sim | 0.17 |
| 416 | agree-quiet | both legs quiet to bound 12 | 0.17 |
| 417 | agree-cex | cex at step 0 reproduced in sim | 0.27 |
| 418 | agree-cex | cex at step 0 reproduced in sim | 0.26 |
| 419 | agree-cex | cex at step 1 reproduced in sim | 0.23 |
| 420 | agree-cex | cex at step 0 reproduced in sim | 0.17 |
| 421 | agree-cex | cex at step 2 reproduced in sim | 0.25 |
| 422 | agree-cex | cex at step 7 reproduced in sim | 0.22 |
| 423 | agree-cex | cex at step 6 reproduced in sim | 0.23 |
| 424 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 425 | agree-cex | cex at step 3 reproduced in sim | 0.24 |
| 426 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 427 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 428 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 429 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 430 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 431 | agree-cex | cex at step 6 reproduced in sim | 0.23 |
| 432 | agree-quiet | both legs quiet to bound 12 | 0.22 |
| 433 | agree-cex | cex at step 0 reproduced in sim | 0.25 |
| 434 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 435 | agree-cex | cex at step 1 reproduced in sim | 0.21 |
| 436 | agree-cex | cex at step 2 reproduced in sim | 0.28 |
| 437 | agree-cex | cex at step 4 reproduced in sim | 0.24 |
| 438 | agree-cex | cex at step 5 reproduced in sim | 0.21 |
| 439 | agree-cex | cex at step 3 reproduced in sim | 0.26 |
| 440 | agree-quiet | both legs quiet to bound 12 | 0.24 |
| 441 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 442 | agree-cex | cex at step 1 reproduced in sim | 0.17 |
| 443 | agree-cex | cex at step 6 reproduced in sim | 0.26 |
| 444 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 445 | agree-quiet | both legs quiet to bound 12 | 0.22 |
| 446 | agree-cex | cex at step 4 reproduced in sim | 0.23 |
| 447 | agree-cex | cex at step 11 reproduced in sim | 0.23 |
| 448 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 449 | agree-quiet | both legs quiet to bound 12 | 0.37 |
| 450 | agree-cex | cex at step 0 reproduced in sim | 0.24 |
| 451 | agree-cex | cex at step 10 reproduced in sim | 0.24 |
| 452 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 453 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 454 | agree-cex | cex at step 5 reproduced in sim | 0.23 |
| 455 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 456 | agree-cex | cex at step 4 reproduced in sim | 0.27 |
| 457 | agree-cex | cex at step 3 reproduced in sim | 0.18 |
| 458 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 459 | agree-cex | cex at step 0 reproduced in sim | 0.24 |
| 460 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 461 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 462 | agree-cex | cex at step 10 reproduced in sim | 0.26 |
| 463 | agree-cex | cex at step 7 reproduced in sim | 0.28 |
| 464 | agree-cex | cex at step 1 reproduced in sim | 0.22 |
| 465 | agree-cex | cex at step 2 reproduced in sim | 0.20 |
| 466 | agree-cex | cex at step 3 reproduced in sim | 0.20 |
| 467 | agree-cex | cex at step 3 reproduced in sim | 0.25 |
| 468 | agree-cex | cex at step 10 reproduced in sim | 0.28 |
| 469 | agree-cex | cex at step 0 reproduced in sim | 0.26 |
| 470 | agree-cex | cex at step 0 reproduced in sim | 0.24 |
| 471 | agree-cex | cex at step 0 reproduced in sim | 0.28 |
| 472 | agree-quiet | both legs quiet to bound 12 | 0.27 |
| 473 | agree-cex | cex at step 2 reproduced in sim | 0.27 |
| 474 | agree-cex | cex at step 2 reproduced in sim | 0.33 |
| 475 | agree-cex | cex at step 5 reproduced in sim | 0.19 |
| 476 | agree-cex | cex at step 7 reproduced in sim | 0.25 |
| 477 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 478 | agree-cex | cex at step 2 reproduced in sim | 0.18 |
| 479 | agree-cex | cex at step 5 reproduced in sim | 0.25 |
| 480 | agree-cex | cex at step 0 reproduced in sim | 0.32 |
| 481 | agree-cex | cex at step 3 reproduced in sim | 0.27 |
| 482 | agree-cex | cex at step 6 reproduced in sim | 0.36 |
| 483 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 484 | agree-cex | cex at step 3 reproduced in sim | 0.23 |
| 485 | agree-cex | cex at step 9 reproduced in sim | 0.25 |
| 486 | agree-cex | cex at step 3 reproduced in sim | 0.24 |
| 487 | agree-cex | cex at step 0 reproduced in sim | 0.18 |
| 488 | agree-cex | cex at step 9 reproduced in sim | 0.27 |
| 489 | agree-cex | cex at step 2 reproduced in sim | 0.21 |
| 490 | agree-cex | cex at step 2 reproduced in sim | 0.22 |
| 491 | agree-quiet | both legs quiet to bound 12 | 0.26 |
| 492 | agree-cex | cex at step 0 reproduced in sim | 0.29 |
| 493 | agree-cex | cex at step 1 reproduced in sim | 0.22 |
| 494 | agree-cex | cex at step 7 reproduced in sim | 0.24 |
| 495 | agree-cex | cex at step 0 reproduced in sim | 0.24 |
| 496 | agree-cex | cex at step 3 reproduced in sim | 0.22 |
| 497 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 498 | agree-cex | cex at step 2 reproduced in sim | 0.20 |
| 499 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 500 | agree-quiet | both legs quiet to bound 12 | 0.22 |
| 501 | agree-cex | cex at step 0 reproduced in sim | 0.26 |
| 502 | agree-quiet | both legs quiet to bound 12 | 0.25 |
| 503 | agree-cex | cex at step 0 reproduced in sim | 0.32 |
| 504 | agree-cex | cex at step 7 reproduced in sim | 0.26 |
| 505 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 506 | agree-cex | cex at step 9 reproduced in sim | 0.23 |
| 507 | agree-cex | cex at step 9 reproduced in sim | 0.18 |
| 508 | agree-cex | cex at step 3 reproduced in sim | 0.21 |
| 509 | agree-cex | cex at step 3 reproduced in sim | 0.25 |
| 510 | agree-cex | cex at step 2 reproduced in sim | 0.24 |
| 511 | agree-cex | cex at step 3 reproduced in sim | 0.29 |
| 512 | agree-quiet | both legs quiet to bound 12 | 0.24 |
| 513 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 514 | agree-cex | cex at step 3 reproduced in sim | 0.23 |
| 515 | agree-cex | cex at step 0 reproduced in sim | 0.25 |
| 516 | agree-cex | cex at step 3 reproduced in sim | 0.22 |
| 517 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 518 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 519 | agree-cex | cex at step 0 reproduced in sim | 0.25 |
| 520 | agree-cex | cex at step 0 reproduced in sim | 0.30 |
| 521 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 522 | agree-quiet | both legs quiet to bound 12 | 0.30 |
| 523 | agree-cex | cex at step 1 reproduced in sim | 0.24 |
| 524 | agree-cex | cex at step 7 reproduced in sim | 0.36 |
| 525 | agree-cex | cex at step 4 reproduced in sim | 0.22 |
| 526 | agree-cex | cex at step 2 reproduced in sim | 0.22 |
| 527 | agree-cex | cex at step 2 reproduced in sim | 0.18 |
| 528 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 529 | agree-cex | cex at step 4 reproduced in sim | 0.23 |
| 530 | agree-cex | cex at step 8 reproduced in sim | 0.21 |
| 531 | agree-cex | cex at step 10 reproduced in sim | 0.31 |
| 532 | agree-cex | cex at step 10 reproduced in sim | 0.25 |
| 533 | agree-cex | cex at step 5 reproduced in sim | 0.25 |
| 534 | agree-cex | cex at step 2 reproduced in sim | 0.19 |
| 535 | agree-cex | cex at step 2 reproduced in sim | 0.19 |
| 536 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 537 | agree-cex | cex at step 3 reproduced in sim | 0.19 |
| 538 | agree-cex | cex at step 5 reproduced in sim | 0.22 |
| 539 | agree-cex | cex at step 9 reproduced in sim | 0.24 |
| 540 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 541 | agree-cex | cex at step 0 reproduced in sim | 0.26 |
| 542 | agree-cex | cex at step 4 reproduced in sim | 0.25 |
| 543 | agree-cex | cex at step 1 reproduced in sim | 0.26 |
| 544 | agree-cex | cex at step 0 reproduced in sim | 0.27 |
| 545 | agree-cex | cex at step 9 reproduced in sim | 0.32 |
| 546 | agree-cex | cex at step 1 reproduced in sim | 0.26 |
| 547 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 548 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 549 | agree-quiet | both legs quiet to bound 12 | 0.22 |
| 550 | agree-cex | cex at step 5 reproduced in sim | 0.21 |
| 551 | agree-cex | cex at step 5 reproduced in sim | 0.25 |
| 552 | agree-cex | cex at step 3 reproduced in sim | 0.22 |
| 553 | agree-cex | cex at step 0 reproduced in sim | 0.27 |
| 554 | agree-cex | cex at step 0 reproduced in sim | 0.24 |
| 555 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 556 | agree-cex | cex at step 2 reproduced in sim | 0.29 |
| 557 | agree-cex | cex at step 5 reproduced in sim | 0.31 |
| 558 | agree-cex | cex at step 8 reproduced in sim | 0.33 |
| 559 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 560 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 561 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 562 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 563 | agree-cex | cex at step 3 reproduced in sim | 0.24 |
| 564 | agree-cex | cex at step 4 reproduced in sim | 0.26 |
| 565 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 566 | agree-cex | cex at step 5 reproduced in sim | 0.24 |
| 567 | agree-cex | cex at step 8 reproduced in sim | 0.28 |
| 568 | agree-cex | cex at step 5 reproduced in sim | 0.26 |
| 569 | agree-cex | cex at step 0 reproduced in sim | 0.25 |
| 570 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 571 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 572 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 573 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 574 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 575 | agree-cex | cex at step 0 reproduced in sim | 0.28 |
| 576 | agree-quiet | both legs quiet to bound 12 | 0.31 |
| 577 | agree-cex | cex at step 2 reproduced in sim | 0.30 |
| 578 | agree-cex | cex at step 10 reproduced in sim | 0.68 |
| 579 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 580 | agree-cex | cex at step 6 reproduced in sim | 0.25 |
| 581 | agree-cex | cex at step 10 reproduced in sim | 0.27 |
| 582 | agree-cex | cex at step 0 reproduced in sim | 0.23 |
| 583 | agree-cex | cex at step 10 reproduced in sim | 0.42 |
| 584 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 585 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 586 | agree-cex | cex at step 2 reproduced in sim | 0.18 |
| 587 | agree-cex | cex at step 2 reproduced in sim | 0.17 |
| 588 | agree-cex | cex at step 3 reproduced in sim | 0.23 |
| 589 | agree-cex | cex at step 11 reproduced in sim | 0.27 |
| 590 | agree-cex | cex at step 1 reproduced in sim | 0.17 |
| 591 | agree-cex | cex at step 0 reproduced in sim | 0.16 |
| 592 | agree-quiet | both legs quiet to bound 12 | 0.30 |
| 593 | agree-cex | cex at step 1 reproduced in sim | 0.21 |
| 594 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 595 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 596 | agree-cex | cex at step 2 reproduced in sim | 0.19 |
| 597 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 598 | agree-cex | cex at step 0 reproduced in sim | 0.16 |
| 599 | agree-quiet | both legs quiet to bound 12 | 0.19 |
