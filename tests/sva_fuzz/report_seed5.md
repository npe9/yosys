# sva_fuzz report -- seed 5, 600 cases, bound 12

- yosys: `/Users/npe/src/sv-yosys/third_party/yosys/build/yosys`
- smtbmc: `/Users/npe/src/sv-yosys/third_party/yosys/build/yosys-smtbmc`, solver z3
- wall time: 17.9 s (0.03 s/case)

## Totals

- cases: 600
- agreement: 600 (quiet 131, cex 469)
- bug-class findings: 0
- determinism guard: PASS (all 600 cases ran the sim leg twice with identical summaries)

## Class C signatures (frontend/lowering gaps, deduped)

- none

## Full per-case log

| index | class | detail | wall s |
|---|---|---|---|
| 0 | agree-cex | cex at step 1 reproduced in sim | 0.23 |
| 1 | agree-cex | cex at step 0 reproduced in sim | 0.28 |
| 2 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 3 | agree-cex | cex at step 6 reproduced in sim | 0.22 |
| 4 | agree-cex | cex at step 3 reproduced in sim | 0.26 |
| 5 | agree-quiet | both legs quiet to bound 12 | 0.24 |
| 6 | agree-cex | cex at step 7 reproduced in sim | 0.27 |
| 7 | agree-cex | cex at step 3 reproduced in sim | 0.26 |
| 8 | agree-cex | cex at step 9 reproduced in sim | 0.22 |
| 9 | agree-cex | cex at step 5 reproduced in sim | 0.20 |
| 10 | agree-quiet | both legs quiet to bound 12 | 0.17 |
| 11 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 12 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 13 | agree-quiet | both legs quiet to bound 12 | 0.25 |
| 14 | agree-cex | cex at step 4 reproduced in sim | 0.24 |
| 15 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 16 | agree-cex | cex at step 3 reproduced in sim | 0.23 |
| 17 | agree-quiet | both legs quiet to bound 12 | 0.20 |
| 18 | agree-cex | cex at step 3 reproduced in sim | 0.18 |
| 19 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 20 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 21 | agree-cex | cex at step 11 reproduced in sim | 0.25 |
| 22 | agree-quiet | both legs quiet to bound 12 | 0.20 |
| 23 | agree-cex | cex at step 3 reproduced in sim | 0.24 |
| 24 | agree-cex | cex at step 1 reproduced in sim | 0.20 |
| 25 | agree-cex | cex at step 3 reproduced in sim | 0.30 |
| 26 | agree-quiet | both legs quiet to bound 12 | 0.20 |
| 27 | agree-cex | cex at step 0 reproduced in sim | 0.18 |
| 28 | agree-cex | cex at step 5 reproduced in sim | 0.21 |
| 29 | agree-quiet | both legs quiet to bound 12 | 0.20 |
| 30 | agree-quiet | both legs quiet to bound 12 | 0.20 |
| 31 | agree-cex | cex at step 1 reproduced in sim | 0.22 |
| 32 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 33 | agree-cex | cex at step 4 reproduced in sim | 0.21 |
| 34 | agree-cex | cex at step 8 reproduced in sim | 0.23 |
| 35 | agree-cex | cex at step 2 reproduced in sim | 0.21 |
| 36 | agree-cex | cex at step 5 reproduced in sim | 0.23 |
| 37 | agree-cex | cex at step 11 reproduced in sim | 0.28 |
| 38 | agree-cex | cex at step 3 reproduced in sim | 0.24 |
| 39 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 40 | agree-cex | cex at step 0 reproduced in sim | 0.18 |
| 41 | agree-quiet | both legs quiet to bound 12 | 0.22 |
| 42 | agree-cex | cex at step 1 reproduced in sim | 0.24 |
| 43 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 44 | agree-quiet | both legs quiet to bound 12 | 0.26 |
| 45 | agree-cex | cex at step 9 reproduced in sim | 0.23 |
| 46 | agree-cex | cex at step 0 reproduced in sim | 0.33 |
| 47 | agree-cex | cex at step 2 reproduced in sim | 0.23 |
| 48 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 49 | agree-cex | cex at step 0 reproduced in sim | 0.18 |
| 50 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 51 | agree-cex | cex at step 1 reproduced in sim | 0.21 |
| 52 | agree-cex | cex at step 6 reproduced in sim | 0.23 |
| 53 | agree-cex | cex at step 0 reproduced in sim | 0.25 |
| 54 | agree-quiet | both legs quiet to bound 12 | 0.28 |
| 55 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 56 | agree-cex | cex at step 0 reproduced in sim | 0.18 |
| 57 | agree-cex | cex at step 3 reproduced in sim | 0.24 |
| 58 | agree-cex | cex at step 2 reproduced in sim | 0.22 |
| 59 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 60 | agree-cex | cex at step 4 reproduced in sim | 0.22 |
| 61 | agree-cex | cex at step 11 reproduced in sim | 0.32 |
| 62 | agree-quiet | both legs quiet to bound 12 | 0.26 |
| 63 | agree-cex | cex at step 2 reproduced in sim | 0.25 |
| 64 | agree-cex | cex at step 9 reproduced in sim | 0.27 |
| 65 | agree-cex | cex at step 6 reproduced in sim | 0.26 |
| 66 | agree-cex | cex at step 4 reproduced in sim | 0.24 |
| 67 | agree-cex | cex at step 0 reproduced in sim | 0.26 |
| 68 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 69 | agree-quiet | both legs quiet to bound 12 | 0.25 |
| 70 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 71 | agree-cex | cex at step 4 reproduced in sim | 0.19 |
| 72 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 73 | agree-cex | cex at step 10 reproduced in sim | 0.22 |
| 74 | agree-cex | cex at step 0 reproduced in sim | 0.23 |
| 75 | agree-quiet | both legs quiet to bound 12 | 0.33 |
| 76 | agree-quiet | both legs quiet to bound 12 | 0.22 |
| 77 | agree-quiet | both legs quiet to bound 12 | 0.20 |
| 78 | agree-cex | cex at step 0 reproduced in sim | 0.18 |
| 79 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 80 | agree-cex | cex at step 6 reproduced in sim | 0.26 |
| 81 | agree-cex | cex at step 5 reproduced in sim | 0.22 |
| 82 | agree-cex | cex at step 1 reproduced in sim | 0.24 |
| 83 | agree-cex | cex at step 2 reproduced in sim | 0.20 |
| 84 | agree-cex | cex at step 3 reproduced in sim | 0.20 |
| 85 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 86 | agree-cex | cex at step 2 reproduced in sim | 0.17 |
| 87 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 88 | agree-cex | cex at step 6 reproduced in sim | 0.27 |
| 89 | agree-cex | cex at step 0 reproduced in sim | 0.25 |
| 90 | agree-cex | cex at step 3 reproduced in sim | 0.28 |
| 91 | agree-cex | cex at step 6 reproduced in sim | 0.32 |
| 92 | agree-cex | cex at step 11 reproduced in sim | 0.23 |
| 93 | agree-cex | cex at step 5 reproduced in sim | 0.22 |
| 94 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 95 | agree-cex | cex at step 9 reproduced in sim | 0.21 |
| 96 | agree-quiet | both legs quiet to bound 12 | 0.17 |
| 97 | agree-cex | cex at step 5 reproduced in sim | 0.18 |
| 98 | agree-cex | cex at step 0 reproduced in sim | 0.17 |
| 99 | agree-cex | cex at step 8 reproduced in sim | 0.28 |
| 100 | agree-quiet | both legs quiet to bound 12 | 0.24 |
| 101 | agree-cex | cex at step 5 reproduced in sim | 0.24 |
| 102 | agree-cex | cex at step 2 reproduced in sim | 0.21 |
| 103 | agree-cex | cex at step 2 reproduced in sim | 0.19 |
| 104 | agree-cex | cex at step 9 reproduced in sim | 0.23 |
| 105 | agree-quiet | both legs quiet to bound 12 | 0.28 |
| 106 | agree-cex | cex at step 3 reproduced in sim | 0.21 |
| 107 | agree-quiet | both legs quiet to bound 12 | 0.16 |
| 108 | agree-cex | cex at step 8 reproduced in sim | 0.27 |
| 109 | agree-cex | cex at step 1 reproduced in sim | 0.24 |
| 110 | agree-cex | cex at step 0 reproduced in sim | 0.25 |
| 111 | agree-cex | cex at step 0 reproduced in sim | 0.24 |
| 112 | agree-cex | cex at step 0 reproduced in sim | 0.17 |
| 113 | agree-cex | cex at step 3 reproduced in sim | 0.17 |
| 114 | agree-cex | cex at step 4 reproduced in sim | 0.20 |
| 115 | agree-cex | cex at step 7 reproduced in sim | 0.19 |
| 116 | agree-cex | cex at step 9 reproduced in sim | 0.31 |
| 117 | agree-cex | cex at step 6 reproduced in sim | 0.22 |
| 118 | agree-cex | cex at step 0 reproduced in sim | 0.26 |
| 119 | agree-cex | cex at step 10 reproduced in sim | 0.36 |
| 120 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 121 | agree-cex | cex at step 4 reproduced in sim | 0.21 |
| 122 | agree-cex | cex at step 0 reproduced in sim | 0.23 |
| 123 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 124 | agree-cex | cex at step 2 reproduced in sim | 0.25 |
| 125 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 126 | agree-cex | cex at step 3 reproduced in sim | 0.27 |
| 127 | agree-cex | cex at step 0 reproduced in sim | 0.26 |
| 128 | agree-cex | cex at step 0 reproduced in sim | 0.26 |
| 129 | agree-quiet | both legs quiet to bound 12 | 0.26 |
| 130 | agree-cex | cex at step 3 reproduced in sim | 0.30 |
| 131 | agree-cex | cex at step 5 reproduced in sim | 0.21 |
| 132 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 133 | agree-quiet | both legs quiet to bound 12 | 0.24 |
| 134 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 135 | agree-cex | cex at step 6 reproduced in sim | 0.22 |
| 136 | agree-cex | cex at step 0 reproduced in sim | 0.26 |
| 137 | agree-cex | cex at step 6 reproduced in sim | 0.21 |
| 138 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 139 | agree-cex | cex at step 3 reproduced in sim | 0.24 |
| 140 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 141 | agree-cex | cex at step 2 reproduced in sim | 0.21 |
| 142 | agree-cex | cex at step 0 reproduced in sim | 0.17 |
| 143 | agree-quiet | both legs quiet to bound 12 | 0.22 |
| 144 | agree-cex | cex at step 3 reproduced in sim | 0.21 |
| 145 | agree-cex | cex at step 9 reproduced in sim | 0.19 |
| 146 | agree-cex | cex at step 11 reproduced in sim | 0.22 |
| 147 | agree-cex | cex at step 0 reproduced in sim | 0.23 |
| 148 | agree-cex | cex at step 0 reproduced in sim | 0.27 |
| 149 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 150 | agree-cex | cex at step 4 reproduced in sim | 0.22 |
| 151 | agree-cex | cex at step 7 reproduced in sim | 0.22 |
| 152 | agree-cex | cex at step 6 reproduced in sim | 0.23 |
| 153 | agree-cex | cex at step 7 reproduced in sim | 0.23 |
| 154 | agree-cex | cex at step 6 reproduced in sim | 0.22 |
| 155 | agree-cex | cex at step 2 reproduced in sim | 0.26 |
| 156 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 157 | agree-cex | cex at step 9 reproduced in sim | 0.27 |
| 158 | agree-quiet | both legs quiet to bound 12 | 0.24 |
| 159 | agree-cex | cex at step 1 reproduced in sim | 0.24 |
| 160 | agree-cex | cex at step 5 reproduced in sim | 0.23 |
| 161 | agree-cex | cex at step 10 reproduced in sim | 0.23 |
| 162 | agree-quiet | both legs quiet to bound 12 | 0.26 |
| 163 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 164 | agree-cex | cex at step 0 reproduced in sim | 0.16 |
| 165 | agree-quiet | both legs quiet to bound 12 | 0.24 |
| 166 | agree-quiet | both legs quiet to bound 12 | 0.17 |
| 167 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 168 | agree-cex | cex at step 7 reproduced in sim | 0.20 |
| 169 | agree-cex | cex at step 8 reproduced in sim | 0.20 |
| 170 | agree-cex | cex at step 0 reproduced in sim | 0.27 |
| 171 | agree-quiet | both legs quiet to bound 12 | 0.20 |
| 172 | agree-quiet | both legs quiet to bound 12 | 0.24 |
| 173 | agree-cex | cex at step 7 reproduced in sim | 0.24 |
| 174 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 175 | agree-quiet | both legs quiet to bound 12 | 0.20 |
| 176 | agree-cex | cex at step 5 reproduced in sim | 0.18 |
| 177 | agree-cex | cex at step 1 reproduced in sim | 0.22 |
| 178 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 179 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 180 | agree-cex | cex at step 3 reproduced in sim | 0.24 |
| 181 | agree-cex | cex at step 1 reproduced in sim | 0.23 |
| 182 | agree-quiet | both legs quiet to bound 12 | 0.26 |
| 183 | agree-cex | cex at step 0 reproduced in sim | 0.23 |
| 184 | agree-quiet | both legs quiet to bound 12 | 0.24 |
| 185 | agree-cex | cex at step 1 reproduced in sim | 0.27 |
| 186 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 187 | agree-cex | cex at step 8 reproduced in sim | 0.27 |
| 188 | agree-cex | cex at step 6 reproduced in sim | 0.25 |
| 189 | agree-quiet | both legs quiet to bound 12 | 0.25 |
| 190 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 191 | agree-quiet | both legs quiet to bound 12 | 0.26 |
| 192 | agree-cex | cex at step 8 reproduced in sim | 0.29 |
| 193 | agree-quiet | both legs quiet to bound 12 | 0.27 |
| 194 | agree-cex | cex at step 7 reproduced in sim | 0.26 |
| 195 | agree-cex | cex at step 2 reproduced in sim | 0.22 |
| 196 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 197 | agree-cex | cex at step 3 reproduced in sim | 0.23 |
| 198 | agree-cex | cex at step 0 reproduced in sim | 0.27 |
| 199 | agree-quiet | both legs quiet to bound 12 | 0.25 |
| 200 | agree-cex | cex at step 8 reproduced in sim | 0.20 |
| 201 | agree-cex | cex at step 7 reproduced in sim | 0.24 |
| 202 | agree-cex | cex at step 4 reproduced in sim | 0.20 |
| 203 | agree-cex | cex at step 8 reproduced in sim | 0.20 |
| 204 | agree-cex | cex at step 9 reproduced in sim | 0.22 |
| 205 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 206 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 207 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 208 | agree-cex | cex at step 4 reproduced in sim | 0.21 |
| 209 | agree-cex | cex at step 2 reproduced in sim | 0.30 |
| 210 | agree-cex | cex at step 1 reproduced in sim | 0.21 |
| 211 | agree-cex | cex at step 2 reproduced in sim | 0.21 |
| 212 | agree-cex | cex at step 2 reproduced in sim | 0.26 |
| 213 | agree-cex | cex at step 6 reproduced in sim | 0.24 |
| 214 | agree-cex | cex at step 1 reproduced in sim | 0.24 |
| 215 | agree-cex | cex at step 0 reproduced in sim | 0.27 |
| 216 | agree-cex | cex at step 3 reproduced in sim | 0.22 |
| 217 | agree-cex | cex at step 6 reproduced in sim | 0.25 |
| 218 | agree-quiet | both legs quiet to bound 12 | 0.30 |
| 219 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 220 | agree-cex | cex at step 0 reproduced in sim | 0.24 |
| 221 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 222 | agree-cex | cex at step 4 reproduced in sim | 0.20 |
| 223 | agree-cex | cex at step 6 reproduced in sim | 0.27 |
| 224 | agree-cex | cex at step 6 reproduced in sim | 0.20 |
| 225 | agree-quiet | both legs quiet to bound 12 | 0.15 |
| 226 | agree-cex | cex at step 5 reproduced in sim | 0.25 |
| 227 | agree-quiet | both legs quiet to bound 12 | 0.24 |
| 228 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 229 | agree-cex | cex at step 0 reproduced in sim | 0.29 |
| 230 | agree-cex | cex at step 10 reproduced in sim | 0.25 |
| 231 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 232 | agree-cex | cex at step 3 reproduced in sim | 0.24 |
| 233 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 234 | agree-cex | cex at step 7 reproduced in sim | 0.20 |
| 235 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 236 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 237 | agree-cex | cex at step 3 reproduced in sim | 0.23 |
| 238 | agree-cex | cex at step 1 reproduced in sim | 0.24 |
| 239 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 240 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 241 | agree-cex | cex at step 3 reproduced in sim | 0.25 |
| 242 | agree-cex | cex at step 8 reproduced in sim | 0.26 |
| 243 | agree-cex | cex at step 0 reproduced in sim | 0.24 |
| 244 | agree-cex | cex at step 0 reproduced in sim | 0.23 |
| 245 | agree-cex | cex at step 0 reproduced in sim | 0.26 |
| 246 | agree-cex | cex at step 8 reproduced in sim | 0.28 |
| 247 | agree-quiet | both legs quiet to bound 12 | 0.24 |
| 248 | agree-cex | cex at step 8 reproduced in sim | 0.31 |
| 249 | agree-cex | cex at step 3 reproduced in sim | 0.25 |
| 250 | agree-cex | cex at step 2 reproduced in sim | 0.23 |
| 251 | agree-cex | cex at step 9 reproduced in sim | 0.25 |
| 252 | agree-cex | cex at step 6 reproduced in sim | 0.28 |
| 253 | agree-cex | cex at step 9 reproduced in sim | 0.24 |
| 254 | agree-cex | cex at step 10 reproduced in sim | 0.22 |
| 255 | agree-cex | cex at step 8 reproduced in sim | 0.21 |
| 256 | agree-quiet | both legs quiet to bound 12 | 0.27 |
| 257 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 258 | agree-cex | cex at step 1 reproduced in sim | 0.22 |
| 259 | agree-cex | cex at step 11 reproduced in sim | 0.29 |
| 260 | agree-cex | cex at step 1 reproduced in sim | 0.27 |
| 261 | agree-cex | cex at step 5 reproduced in sim | 0.20 |
| 262 | agree-cex | cex at step 0 reproduced in sim | 0.18 |
| 263 | agree-cex | cex at step 8 reproduced in sim | 0.24 |
| 264 | agree-quiet | both legs quiet to bound 12 | 0.27 |
| 265 | agree-cex | cex at step 9 reproduced in sim | 0.24 |
| 266 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 267 | agree-quiet | both legs quiet to bound 12 | 0.26 |
| 268 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 269 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 270 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 271 | agree-cex | cex at step 4 reproduced in sim | 0.18 |
| 272 | agree-cex | cex at step 3 reproduced in sim | 0.23 |
| 273 | agree-cex | cex at step 8 reproduced in sim | 0.19 |
| 274 | agree-cex | cex at step 0 reproduced in sim | 0.18 |
| 275 | agree-cex | cex at step 6 reproduced in sim | 0.24 |
| 276 | agree-cex | cex at step 2 reproduced in sim | 0.20 |
| 277 | agree-cex | cex at step 8 reproduced in sim | 0.23 |
| 278 | agree-quiet | both legs quiet to bound 12 | 0.17 |
| 279 | agree-quiet | both legs quiet to bound 12 | 0.63 |
| 280 | agree-cex | cex at step 11 reproduced in sim | 0.27 |
| 281 | agree-cex | cex at step 8 reproduced in sim | 0.23 |
| 282 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 283 | agree-cex | cex at step 2 reproduced in sim | 0.21 |
| 284 | agree-cex | cex at step 5 reproduced in sim | 0.23 |
| 285 | agree-cex | cex at step 0 reproduced in sim | 0.17 |
| 286 | agree-cex | cex at step 7 reproduced in sim | 0.20 |
| 287 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 288 | agree-cex | cex at step 11 reproduced in sim | 0.29 |
| 289 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 290 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 291 | agree-cex | cex at step 7 reproduced in sim | 0.22 |
| 292 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 293 | agree-cex | cex at step 1 reproduced in sim | 0.19 |
| 294 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 295 | agree-quiet | both legs quiet to bound 12 | 0.22 |
| 296 | agree-cex | cex at step 0 reproduced in sim | 0.17 |
| 297 | agree-cex | cex at step 7 reproduced in sim | 0.29 |
| 298 | agree-cex | cex at step 0 reproduced in sim | 0.26 |
| 299 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 300 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 301 | agree-cex | cex at step 1 reproduced in sim | 0.20 |
| 302 | agree-cex | cex at step 3 reproduced in sim | 0.21 |
| 303 | agree-quiet | both legs quiet to bound 12 | 0.22 |
| 304 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 305 | agree-quiet | both legs quiet to bound 12 | 0.22 |
| 306 | agree-cex | cex at step 11 reproduced in sim | 0.24 |
| 307 | agree-cex | cex at step 3 reproduced in sim | 0.22 |
| 308 | agree-cex | cex at step 11 reproduced in sim | 0.31 |
| 309 | agree-quiet | both legs quiet to bound 12 | 0.24 |
| 310 | agree-quiet | both legs quiet to bound 12 | 0.20 |
| 311 | agree-cex | cex at step 1 reproduced in sim | 0.22 |
| 312 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 313 | agree-cex | cex at step 2 reproduced in sim | 0.24 |
| 314 | agree-cex | cex at step 6 reproduced in sim | 0.23 |
| 315 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 316 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 317 | agree-cex | cex at step 0 reproduced in sim | 0.23 |
| 318 | agree-cex | cex at step 2 reproduced in sim | 0.23 |
| 319 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 320 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 321 | agree-cex | cex at step 1 reproduced in sim | 0.23 |
| 322 | agree-quiet | both legs quiet to bound 12 | 0.20 |
| 323 | agree-cex | cex at step 7 reproduced in sim | 0.28 |
| 324 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 325 | agree-cex | cex at step 11 reproduced in sim | 0.28 |
| 326 | agree-cex | cex at step 1 reproduced in sim | 0.23 |
| 327 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 328 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 329 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 330 | agree-cex | cex at step 0 reproduced in sim | 0.23 |
| 331 | agree-cex | cex at step 5 reproduced in sim | 0.20 |
| 332 | agree-cex | cex at step 11 reproduced in sim | 0.21 |
| 333 | agree-cex | cex at step 3 reproduced in sim | 0.23 |
| 334 | agree-quiet | both legs quiet to bound 12 | 0.30 |
| 335 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 336 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 337 | agree-cex | cex at step 2 reproduced in sim | 0.21 |
| 338 | agree-cex | cex at step 4 reproduced in sim | 0.22 |
| 339 | agree-cex | cex at step 10 reproduced in sim | 0.27 |
| 340 | agree-quiet | both legs quiet to bound 12 | 0.32 |
| 341 | agree-cex | cex at step 0 reproduced in sim | 0.28 |
| 342 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 343 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 344 | agree-quiet | both legs quiet to bound 12 | 0.27 |
| 345 | agree-cex | cex at step 0 reproduced in sim | 0.24 |
| 346 | agree-cex | cex at step 8 reproduced in sim | 0.26 |
| 347 | agree-cex | cex at step 2 reproduced in sim | 0.21 |
| 348 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 349 | agree-cex | cex at step 2 reproduced in sim | 0.32 |
| 350 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 351 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 352 | agree-cex | cex at step 7 reproduced in sim | 0.30 |
| 353 | agree-cex | cex at step 4 reproduced in sim | 0.24 |
| 354 | agree-cex | cex at step 8 reproduced in sim | 0.25 |
| 355 | agree-cex | cex at step 2 reproduced in sim | 0.27 |
| 356 | agree-cex | cex at step 5 reproduced in sim | 0.22 |
| 357 | agree-cex | cex at step 11 reproduced in sim | 0.28 |
| 358 | agree-quiet | both legs quiet to bound 12 | 0.22 |
| 359 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 360 | agree-cex | cex at step 6 reproduced in sim | 0.24 |
| 361 | agree-cex | cex at step 2 reproduced in sim | 0.24 |
| 362 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 363 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 364 | agree-cex | cex at step 1 reproduced in sim | 0.28 |
| 365 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 366 | agree-cex | cex at step 4 reproduced in sim | 0.27 |
| 367 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 368 | agree-cex | cex at step 0 reproduced in sim | 0.23 |
| 369 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 370 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 371 | agree-quiet | both legs quiet to bound 12 | 0.25 |
| 372 | agree-cex | cex at step 9 reproduced in sim | 0.25 |
| 373 | agree-cex | cex at step 2 reproduced in sim | 0.26 |
| 374 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 375 | agree-quiet | both legs quiet to bound 12 | 0.24 |
| 376 | agree-cex | cex at step 6 reproduced in sim | 0.24 |
| 377 | agree-cex | cex at step 5 reproduced in sim | 0.29 |
| 378 | agree-cex | cex at step 0 reproduced in sim | 0.24 |
| 379 | agree-quiet | both legs quiet to bound 12 | 0.27 |
| 380 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 381 | agree-cex | cex at step 2 reproduced in sim | 0.22 |
| 382 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 383 | agree-cex | cex at step 4 reproduced in sim | 0.28 |
| 384 | agree-cex | cex at step 4 reproduced in sim | 0.24 |
| 385 | agree-cex | cex at step 4 reproduced in sim | 0.23 |
| 386 | agree-cex | cex at step 8 reproduced in sim | 0.20 |
| 387 | agree-quiet | both legs quiet to bound 12 | 0.22 |
| 388 | agree-cex | cex at step 7 reproduced in sim | 0.30 |
| 389 | agree-cex | cex at step 3 reproduced in sim | 0.24 |
| 390 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 391 | agree-cex | cex at step 1 reproduced in sim | 0.24 |
| 392 | agree-cex | cex at step 2 reproduced in sim | 0.20 |
| 393 | agree-quiet | both legs quiet to bound 12 | 0.25 |
| 394 | agree-cex | cex at step 3 reproduced in sim | 0.19 |
| 395 | agree-cex | cex at step 2 reproduced in sim | 0.25 |
| 396 | agree-cex | cex at step 7 reproduced in sim | 0.23 |
| 397 | agree-cex | cex at step 3 reproduced in sim | 0.23 |
| 398 | agree-cex | cex at step 2 reproduced in sim | 0.21 |
| 399 | agree-cex | cex at step 5 reproduced in sim | 0.25 |
| 400 | agree-cex | cex at step 0 reproduced in sim | 0.24 |
| 401 | agree-cex | cex at step 3 reproduced in sim | 0.24 |
| 402 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 403 | agree-cex | cex at step 2 reproduced in sim | 0.21 |
| 404 | agree-cex | cex at step 6 reproduced in sim | 0.23 |
| 405 | agree-quiet | both legs quiet to bound 12 | 0.22 |
| 406 | agree-cex | cex at step 1 reproduced in sim | 0.20 |
| 407 | agree-cex | cex at step 10 reproduced in sim | 0.30 |
| 408 | agree-cex | cex at step 5 reproduced in sim | 0.21 |
| 409 | agree-cex | cex at step 1 reproduced in sim | 0.22 |
| 410 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 411 | agree-quiet | both legs quiet to bound 12 | 0.20 |
| 412 | agree-cex | cex at step 0 reproduced in sim | 0.18 |
| 413 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 414 | agree-quiet | both legs quiet to bound 12 | 0.20 |
| 415 | agree-cex | cex at step 1 reproduced in sim | 0.20 |
| 416 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 417 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 418 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 419 | agree-cex | cex at step 2 reproduced in sim | 0.25 |
| 420 | agree-cex | cex at step 7 reproduced in sim | 0.25 |
| 421 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 422 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 423 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 424 | agree-cex | cex at step 3 reproduced in sim | 0.19 |
| 425 | agree-cex | cex at step 3 reproduced in sim | 0.25 |
| 426 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 427 | agree-cex | cex at step 8 reproduced in sim | 0.24 |
| 428 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 429 | agree-cex | cex at step 8 reproduced in sim | 0.25 |
| 430 | agree-cex | cex at step 0 reproduced in sim | 0.25 |
| 431 | agree-quiet | both legs quiet to bound 12 | 0.29 |
| 432 | agree-cex | cex at step 2 reproduced in sim | 0.24 |
| 433 | agree-cex | cex at step 8 reproduced in sim | 0.22 |
| 434 | agree-cex | cex at step 2 reproduced in sim | 0.23 |
| 435 | agree-cex | cex at step 8 reproduced in sim | 0.27 |
| 436 | agree-cex | cex at step 0 reproduced in sim | 0.18 |
| 437 | agree-quiet | both legs quiet to bound 12 | 0.33 |
| 438 | agree-cex | cex at step 4 reproduced in sim | 0.22 |
| 439 | agree-cex | cex at step 6 reproduced in sim | 0.24 |
| 440 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 441 | agree-cex | cex at step 4 reproduced in sim | 0.24 |
| 442 | agree-cex | cex at step 8 reproduced in sim | 0.21 |
| 443 | agree-quiet | both legs quiet to bound 12 | 0.36 |
| 444 | agree-cex | cex at step 3 reproduced in sim | 0.22 |
| 445 | agree-cex | cex at step 7 reproduced in sim | 0.22 |
| 446 | agree-cex | cex at step 9 reproduced in sim | 0.22 |
| 447 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 448 | agree-cex | cex at step 9 reproduced in sim | 0.22 |
| 449 | agree-quiet | both legs quiet to bound 12 | 0.24 |
| 450 | agree-cex | cex at step 3 reproduced in sim | 0.24 |
| 451 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 452 | agree-cex | cex at step 1 reproduced in sim | 0.23 |
| 453 | agree-cex | cex at step 8 reproduced in sim | 0.22 |
| 454 | agree-cex | cex at step 5 reproduced in sim | 0.29 |
| 455 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 456 | agree-quiet | both legs quiet to bound 12 | 0.22 |
| 457 | agree-cex | cex at step 1 reproduced in sim | 0.25 |
| 458 | agree-quiet | both legs quiet to bound 12 | 0.27 |
| 459 | agree-quiet | both legs quiet to bound 12 | 0.24 |
| 460 | agree-cex | cex at step 3 reproduced in sim | 0.27 |
| 461 | agree-cex | cex at step 10 reproduced in sim | 0.29 |
| 462 | agree-cex | cex at step 0 reproduced in sim | 0.28 |
| 463 | agree-quiet | both legs quiet to bound 12 | 0.24 |
| 464 | agree-cex | cex at step 4 reproduced in sim | 0.21 |
| 465 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 466 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 467 | agree-cex | cex at step 1 reproduced in sim | 0.21 |
| 468 | agree-cex | cex at step 1 reproduced in sim | 0.24 |
| 469 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 470 | agree-quiet | both legs quiet to bound 12 | 0.16 |
| 471 | agree-cex | cex at step 10 reproduced in sim | 0.25 |
| 472 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 473 | agree-cex | cex at step 8 reproduced in sim | 0.24 |
| 474 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 475 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 476 | agree-cex | cex at step 1 reproduced in sim | 0.21 |
| 477 | agree-quiet | both legs quiet to bound 12 | 0.17 |
| 478 | agree-cex | cex at step 2 reproduced in sim | 0.20 |
| 479 | agree-cex | cex at step 2 reproduced in sim | 0.22 |
| 480 | agree-cex | cex at step 5 reproduced in sim | 0.28 |
| 481 | agree-cex | cex at step 10 reproduced in sim | 0.26 |
| 482 | agree-cex | cex at step 0 reproduced in sim | 0.24 |
| 483 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 484 | agree-cex | cex at step 0 reproduced in sim | 0.23 |
| 485 | agree-cex | cex at step 3 reproduced in sim | 0.22 |
| 486 | agree-cex | cex at step 4 reproduced in sim | 0.27 |
| 487 | agree-cex | cex at step 3 reproduced in sim | 0.23 |
| 488 | agree-quiet | both legs quiet to bound 12 | 0.24 |
| 489 | agree-cex | cex at step 7 reproduced in sim | 0.32 |
| 490 | agree-cex | cex at step 10 reproduced in sim | 0.25 |
| 491 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 492 | agree-cex | cex at step 1 reproduced in sim | 0.26 |
| 493 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 494 | agree-cex | cex at step 0 reproduced in sim | 0.25 |
| 495 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 496 | agree-cex | cex at step 11 reproduced in sim | 0.27 |
| 497 | agree-cex | cex at step 3 reproduced in sim | 0.24 |
| 498 | agree-cex | cex at step 2 reproduced in sim | 0.24 |
| 499 | agree-cex | cex at step 7 reproduced in sim | 0.30 |
| 500 | agree-cex | cex at step 8 reproduced in sim | 0.20 |
| 501 | agree-cex | cex at step 0 reproduced in sim | 0.23 |
| 502 | agree-cex | cex at step 8 reproduced in sim | 0.27 |
| 503 | agree-cex | cex at step 4 reproduced in sim | 0.24 |
| 504 | agree-cex | cex at step 2 reproduced in sim | 0.25 |
| 505 | agree-cex | cex at step 6 reproduced in sim | 0.29 |
| 506 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 507 | agree-cex | cex at step 0 reproduced in sim | 0.25 |
| 508 | agree-cex | cex at step 2 reproduced in sim | 0.22 |
| 509 | agree-quiet | both legs quiet to bound 12 | 0.30 |
| 510 | agree-cex | cex at step 6 reproduced in sim | 0.22 |
| 511 | agree-cex | cex at step 2 reproduced in sim | 0.20 |
| 512 | agree-cex | cex at step 2 reproduced in sim | 0.18 |
| 513 | agree-cex | cex at step 3 reproduced in sim | 0.22 |
| 514 | agree-quiet | both legs quiet to bound 12 | 0.20 |
| 515 | agree-cex | cex at step 6 reproduced in sim | 0.26 |
| 516 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 517 | agree-cex | cex at step 0 reproduced in sim | 0.25 |
| 518 | agree-cex | cex at step 6 reproduced in sim | 0.23 |
| 519 | agree-cex | cex at step 8 reproduced in sim | 0.26 |
| 520 | agree-cex | cex at step 11 reproduced in sim | 0.26 |
| 521 | agree-cex | cex at step 4 reproduced in sim | 0.27 |
| 522 | agree-quiet | both legs quiet to bound 12 | 0.25 |
| 523 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 524 | agree-cex | cex at step 7 reproduced in sim | 0.25 |
| 525 | agree-cex | cex at step 3 reproduced in sim | 0.23 |
| 526 | agree-cex | cex at step 0 reproduced in sim | 0.24 |
| 527 | agree-cex | cex at step 9 reproduced in sim | 0.24 |
| 528 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 529 | agree-cex | cex at step 5 reproduced in sim | 0.21 |
| 530 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 531 | agree-quiet | both legs quiet to bound 12 | 0.26 |
| 532 | agree-cex | cex at step 5 reproduced in sim | 0.22 |
| 533 | agree-cex | cex at step 1 reproduced in sim | 0.24 |
| 534 | agree-cex | cex at step 3 reproduced in sim | 0.27 |
| 535 | agree-cex | cex at step 1 reproduced in sim | 0.23 |
| 536 | agree-cex | cex at step 3 reproduced in sim | 0.25 |
| 537 | agree-cex | cex at step 5 reproduced in sim | 0.24 |
| 538 | agree-cex | cex at step 9 reproduced in sim | 0.23 |
| 539 | agree-cex | cex at step 3 reproduced in sim | 0.21 |
| 540 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 541 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 542 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 543 | agree-cex | cex at step 1 reproduced in sim | 0.22 |
| 544 | agree-quiet | both legs quiet to bound 12 | 0.26 |
| 545 | agree-quiet | both legs quiet to bound 12 | 0.33 |
| 546 | agree-cex | cex at step 3 reproduced in sim | 0.22 |
| 547 | agree-cex | cex at step 5 reproduced in sim | 0.21 |
| 548 | agree-quiet | both legs quiet to bound 12 | 0.27 |
| 549 | agree-cex | cex at step 0 reproduced in sim | 0.24 |
| 550 | agree-cex | cex at step 1 reproduced in sim | 0.24 |
| 551 | agree-cex | cex at step 0 reproduced in sim | 0.25 |
| 552 | agree-cex | cex at step 8 reproduced in sim | 0.21 |
| 553 | agree-cex | cex at step 10 reproduced in sim | 0.26 |
| 554 | agree-cex | cex at step 4 reproduced in sim | 0.22 |
| 555 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 556 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 557 | agree-cex | cex at step 2 reproduced in sim | 0.23 |
| 558 | agree-cex | cex at step 4 reproduced in sim | 0.19 |
| 559 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 560 | agree-cex | cex at step 0 reproduced in sim | 0.17 |
| 561 | agree-cex | cex at step 0 reproduced in sim | 0.24 |
| 562 | agree-cex | cex at step 0 reproduced in sim | 0.18 |
| 563 | agree-quiet | both legs quiet to bound 12 | 0.25 |
| 564 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 565 | agree-cex | cex at step 0 reproduced in sim | 0.26 |
| 566 | agree-cex | cex at step 1 reproduced in sim | 0.22 |
| 567 | agree-cex | cex at step 1 reproduced in sim | 0.24 |
| 568 | agree-cex | cex at step 2 reproduced in sim | 0.28 |
| 569 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 570 | agree-cex | cex at step 4 reproduced in sim | 0.22 |
| 571 | agree-cex | cex at step 0 reproduced in sim | 0.26 |
| 572 | agree-cex | cex at step 4 reproduced in sim | 0.22 |
| 573 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 574 | agree-cex | cex at step 2 reproduced in sim | 0.19 |
| 575 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 576 | agree-cex | cex at step 3 reproduced in sim | 0.22 |
| 577 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 578 | agree-cex | cex at step 11 reproduced in sim | 0.24 |
| 579 | agree-cex | cex at step 9 reproduced in sim | 0.23 |
| 580 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 581 | agree-cex | cex at step 0 reproduced in sim | 0.23 |
| 582 | agree-quiet | both legs quiet to bound 12 | 0.31 |
| 583 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 584 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 585 | agree-cex | cex at step 4 reproduced in sim | 0.22 |
| 586 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 587 | agree-cex | cex at step 2 reproduced in sim | 0.19 |
| 588 | agree-cex | cex at step 11 reproduced in sim | 0.22 |
| 589 | agree-cex | cex at step 3 reproduced in sim | 0.24 |
| 590 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 591 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 592 | agree-cex | cex at step 10 reproduced in sim | 0.25 |
| 593 | agree-cex | cex at step 0 reproduced in sim | 0.24 |
| 594 | agree-cex | cex at step 0 reproduced in sim | 0.18 |
| 595 | agree-cex | cex at step 6 reproduced in sim | 0.24 |
| 596 | agree-cex | cex at step 6 reproduced in sim | 0.21 |
| 597 | agree-cex | cex at step 0 reproduced in sim | 0.18 |
| 598 | agree-cex | cex at step 5 reproduced in sim | 0.21 |
| 599 | agree-cex | cex at step 0 reproduced in sim | 0.17 |
