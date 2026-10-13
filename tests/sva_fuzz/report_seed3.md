# sva_fuzz report -- seed 3, 600 cases, bound 12

- yosys: `/Users/npe/src/sv-yosys/third_party/yosys/build/yosys`
- smtbmc: `/Users/npe/src/sv-yosys/third_party/yosys/build/yosys-smtbmc`, solver z3
- wall time: 17.5 s (0.03 s/case)

## Totals

- cases: 600
- agreement: 600 (quiet 150, cex 450)
- bug-class findings: 0
- determinism guard: PASS (all 600 cases ran the sim leg twice with identical summaries)

## Class C signatures (frontend/lowering gaps, deduped)

- none

## Full per-case log

| index | class | detail | wall s |
|---|---|---|---|
| 0 | agree-cex | cex at step 3 reproduced in sim | 0.24 |
| 1 | agree-cex | cex at step 3 reproduced in sim | 0.25 |
| 2 | agree-quiet | both legs quiet to bound 12 | 0.20 |
| 3 | agree-cex | cex at step 6 reproduced in sim | 0.23 |
| 4 | agree-quiet | both legs quiet to bound 12 | 0.26 |
| 5 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 6 | agree-cex | cex at step 2 reproduced in sim | 0.24 |
| 7 | agree-quiet | both legs quiet to bound 12 | 0.24 |
| 8 | agree-quiet | both legs quiet to bound 12 | 0.26 |
| 9 | agree-cex | cex at step 0 reproduced in sim | 0.23 |
| 10 | agree-quiet | both legs quiet to bound 12 | 0.24 |
| 11 | agree-cex | cex at step 0 reproduced in sim | 0.25 |
| 12 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 13 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 14 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 15 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 16 | agree-cex | cex at step 0 reproduced in sim | 0.26 |
| 17 | agree-cex | cex at step 5 reproduced in sim | 0.22 |
| 18 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 19 | agree-quiet | both legs quiet to bound 12 | 0.20 |
| 20 | agree-quiet | both legs quiet to bound 12 | 0.17 |
| 21 | agree-cex | cex at step 6 reproduced in sim | 0.21 |
| 22 | agree-cex | cex at step 8 reproduced in sim | 0.25 |
| 23 | agree-cex | cex at step 1 reproduced in sim | 0.23 |
| 24 | agree-cex | cex at step 3 reproduced in sim | 0.21 |
| 25 | agree-cex | cex at step 4 reproduced in sim | 0.25 |
| 26 | agree-cex | cex at step 0 reproduced in sim | 0.18 |
| 27 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 28 | agree-cex | cex at step 0 reproduced in sim | 0.16 |
| 29 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 30 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 31 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 32 | agree-cex | cex at step 1 reproduced in sim | 0.27 |
| 33 | agree-cex | cex at step 11 reproduced in sim | 0.27 |
| 34 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 35 | agree-cex | cex at step 3 reproduced in sim | 0.30 |
| 36 | agree-quiet | both legs quiet to bound 12 | 0.25 |
| 37 | agree-cex | cex at step 6 reproduced in sim | 0.25 |
| 38 | agree-cex | cex at step 4 reproduced in sim | 0.26 |
| 39 | agree-cex | cex at step 3 reproduced in sim | 0.19 |
| 40 | agree-cex | cex at step 0 reproduced in sim | 0.18 |
| 41 | agree-cex | cex at step 1 reproduced in sim | 0.28 |
| 42 | agree-cex | cex at step 7 reproduced in sim | 0.19 |
| 43 | agree-cex | cex at step 0 reproduced in sim | 0.16 |
| 44 | agree-cex | cex at step 7 reproduced in sim | 0.17 |
| 45 | agree-cex | cex at step 11 reproduced in sim | 0.16 |
| 46 | agree-cex | cex at step 3 reproduced in sim | 0.21 |
| 47 | agree-cex | cex at step 7 reproduced in sim | 0.17 |
| 48 | agree-cex | cex at step 5 reproduced in sim | 0.26 |
| 49 | agree-cex | cex at step 4 reproduced in sim | 0.15 |
| 50 | agree-cex | cex at step 8 reproduced in sim | 0.20 |
| 51 | agree-quiet | both legs quiet to bound 12 | 0.15 |
| 52 | agree-cex | cex at step 2 reproduced in sim | 0.16 |
| 53 | agree-quiet | both legs quiet to bound 12 | 0.14 |
| 54 | agree-cex | cex at step 4 reproduced in sim | 0.19 |
| 55 | agree-cex | cex at step 3 reproduced in sim | 0.20 |
| 56 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 57 | agree-cex | cex at step 3 reproduced in sim | 0.17 |
| 58 | agree-cex | cex at step 8 reproduced in sim | 0.26 |
| 59 | agree-cex | cex at step 5 reproduced in sim | 0.17 |
| 60 | agree-cex | cex at step 0 reproduced in sim | 0.24 |
| 61 | agree-quiet | both legs quiet to bound 12 | 0.17 |
| 62 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 63 | agree-cex | cex at step 4 reproduced in sim | 0.17 |
| 64 | agree-cex | cex at step 2 reproduced in sim | 0.18 |
| 65 | agree-cex | cex at step 3 reproduced in sim | 0.23 |
| 66 | agree-cex | cex at step 3 reproduced in sim | 0.20 |
| 67 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 68 | agree-cex | cex at step 0 reproduced in sim | 0.18 |
| 69 | agree-cex | cex at step 7 reproduced in sim | 0.24 |
| 70 | agree-cex | cex at step 0 reproduced in sim | 0.16 |
| 71 | agree-cex | cex at step 6 reproduced in sim | 0.18 |
| 72 | agree-cex | cex at step 2 reproduced in sim | 0.20 |
| 73 | agree-cex | cex at step 4 reproduced in sim | 0.18 |
| 74 | agree-cex | cex at step 1 reproduced in sim | 0.16 |
| 75 | agree-cex | cex at step 1 reproduced in sim | 0.20 |
| 76 | agree-cex | cex at step 0 reproduced in sim | 0.16 |
| 77 | agree-cex | cex at step 0 reproduced in sim | 0.15 |
| 78 | agree-cex | cex at step 3 reproduced in sim | 0.15 |
| 79 | agree-cex | cex at step 3 reproduced in sim | 0.17 |
| 80 | agree-quiet | both legs quiet to bound 12 | 0.15 |
| 81 | agree-cex | cex at step 6 reproduced in sim | 0.20 |
| 82 | agree-cex | cex at step 6 reproduced in sim | 0.20 |
| 83 | agree-cex | cex at step 2 reproduced in sim | 0.17 |
| 84 | agree-quiet | both legs quiet to bound 12 | 0.27 |
| 85 | agree-cex | cex at step 0 reproduced in sim | 0.27 |
| 86 | agree-cex | cex at step 5 reproduced in sim | 0.25 |
| 87 | agree-cex | cex at step 1 reproduced in sim | 0.20 |
| 88 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 89 | agree-cex | cex at step 8 reproduced in sim | 0.21 |
| 90 | agree-cex | cex at step 0 reproduced in sim | 0.18 |
| 91 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 92 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 93 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 94 | agree-cex | cex at step 6 reproduced in sim | 0.22 |
| 95 | agree-quiet | both legs quiet to bound 12 | 0.26 |
| 96 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 97 | agree-cex | cex at step 3 reproduced in sim | 0.22 |
| 98 | agree-cex | cex at step 0 reproduced in sim | 0.24 |
| 99 | agree-cex | cex at step 1 reproduced in sim | 0.19 |
| 100 | agree-quiet | both legs quiet to bound 12 | 0.12 |
| 101 | agree-cex | cex at step 5 reproduced in sim | 0.16 |
| 102 | agree-cex | cex at step 0 reproduced in sim | 0.15 |
| 103 | agree-cex | cex at step 0 reproduced in sim | 0.16 |
| 104 | agree-cex | cex at step 0 reproduced in sim | 0.16 |
| 105 | agree-quiet | both legs quiet to bound 12 | 0.20 |
| 106 | agree-cex | cex at step 5 reproduced in sim | 0.21 |
| 107 | agree-cex | cex at step 3 reproduced in sim | 0.25 |
| 108 | agree-quiet | both legs quiet to bound 12 | 0.22 |
| 109 | agree-cex | cex at step 4 reproduced in sim | 0.24 |
| 110 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 111 | agree-cex | cex at step 4 reproduced in sim | 0.20 |
| 112 | agree-cex | cex at step 1 reproduced in sim | 0.22 |
| 113 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 114 | agree-cex | cex at step 11 reproduced in sim | 0.24 |
| 115 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 116 | agree-cex | cex at step 5 reproduced in sim | 0.19 |
| 117 | agree-cex | cex at step 8 reproduced in sim | 0.23 |
| 118 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 119 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 120 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 121 | agree-cex | cex at step 5 reproduced in sim | 0.19 |
| 122 | agree-cex | cex at step 1 reproduced in sim | 0.17 |
| 123 | agree-cex | cex at step 0 reproduced in sim | 0.18 |
| 124 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 125 | agree-cex | cex at step 0 reproduced in sim | 0.18 |
| 126 | agree-quiet | both legs quiet to bound 12 | 0.16 |
| 127 | agree-cex | cex at step 8 reproduced in sim | 0.20 |
| 128 | agree-cex | cex at step 4 reproduced in sim | 0.20 |
| 129 | agree-cex | cex at step 0 reproduced in sim | 0.17 |
| 130 | agree-cex | cex at step 6 reproduced in sim | 0.21 |
| 131 | agree-cex | cex at step 4 reproduced in sim | 0.25 |
| 132 | agree-cex | cex at step 3 reproduced in sim | 0.21 |
| 133 | agree-quiet | both legs quiet to bound 12 | 0.16 |
| 134 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 135 | agree-cex | cex at step 6 reproduced in sim | 0.23 |
| 136 | agree-cex | cex at step 2 reproduced in sim | 0.23 |
| 137 | agree-cex | cex at step 3 reproduced in sim | 0.21 |
| 138 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 139 | agree-quiet | both legs quiet to bound 12 | 0.28 |
| 140 | agree-cex | cex at step 3 reproduced in sim | 0.24 |
| 141 | agree-cex | cex at step 0 reproduced in sim | 0.32 |
| 142 | agree-quiet | both legs quiet to bound 12 | 0.25 |
| 143 | agree-cex | cex at step 0 reproduced in sim | 0.25 |
| 144 | agree-quiet | both legs quiet to bound 12 | 0.24 |
| 145 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 146 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 147 | agree-cex | cex at step 3 reproduced in sim | 0.19 |
| 148 | agree-cex | cex at step 9 reproduced in sim | 0.21 |
| 149 | agree-cex | cex at step 9 reproduced in sim | 0.17 |
| 150 | agree-cex | cex at step 1 reproduced in sim | 0.17 |
| 151 | agree-quiet | both legs quiet to bound 12 | 0.20 |
| 152 | agree-cex | cex at step 0 reproduced in sim | 0.18 |
| 153 | agree-cex | cex at step 4 reproduced in sim | 0.16 |
| 154 | agree-cex | cex at step 4 reproduced in sim | 0.20 |
| 155 | agree-cex | cex at step 3 reproduced in sim | 0.17 |
| 156 | agree-cex | cex at step 2 reproduced in sim | 0.16 |
| 157 | agree-quiet | both legs quiet to bound 12 | 0.15 |
| 158 | agree-cex | cex at step 1 reproduced in sim | 0.18 |
| 159 | agree-cex | cex at step 10 reproduced in sim | 0.23 |
| 160 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 161 | agree-quiet | both legs quiet to bound 12 | 0.32 |
| 162 | agree-cex | cex at step 4 reproduced in sim | 0.22 |
| 163 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 164 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 165 | agree-cex | cex at step 0 reproduced in sim | 0.31 |
| 166 | agree-cex | cex at step 3 reproduced in sim | 0.24 |
| 167 | agree-cex | cex at step 9 reproduced in sim | 0.26 |
| 168 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 169 | agree-cex | cex at step 5 reproduced in sim | 0.26 |
| 170 | agree-cex | cex at step 4 reproduced in sim | 0.23 |
| 171 | agree-quiet | both legs quiet to bound 12 | 0.17 |
| 172 | agree-cex | cex at step 4 reproduced in sim | 0.19 |
| 173 | agree-cex | cex at step 3 reproduced in sim | 0.17 |
| 174 | agree-cex | cex at step 11 reproduced in sim | 0.22 |
| 175 | agree-quiet | both legs quiet to bound 12 | 0.20 |
| 176 | agree-cex | cex at step 11 reproduced in sim | 0.22 |
| 177 | agree-cex | cex at step 5 reproduced in sim | 0.17 |
| 178 | agree-cex | cex at step 8 reproduced in sim | 0.23 |
| 179 | agree-cex | cex at step 0 reproduced in sim | 0.18 |
| 180 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 181 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 182 | agree-cex | cex at step 7 reproduced in sim | 0.19 |
| 183 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 184 | agree-cex | cex at step 10 reproduced in sim | 0.22 |
| 185 | agree-quiet | both legs quiet to bound 12 | 0.20 |
| 186 | agree-cex | cex at step 3 reproduced in sim | 0.24 |
| 187 | agree-cex | cex at step 0 reproduced in sim | 0.17 |
| 188 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 189 | agree-cex | cex at step 1 reproduced in sim | 0.24 |
| 190 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 191 | agree-cex | cex at step 3 reproduced in sim | 0.19 |
| 192 | agree-cex | cex at step 3 reproduced in sim | 0.20 |
| 193 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 194 | agree-cex | cex at step 5 reproduced in sim | 0.27 |
| 195 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 196 | agree-cex | cex at step 4 reproduced in sim | 0.21 |
| 197 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 198 | agree-cex | cex at step 0 reproduced in sim | 0.27 |
| 199 | agree-cex | cex at step 0 reproduced in sim | 0.25 |
| 200 | agree-quiet | both legs quiet to bound 12 | 0.22 |
| 201 | agree-cex | cex at step 5 reproduced in sim | 0.32 |
| 202 | agree-cex | cex at step 5 reproduced in sim | 0.27 |
| 203 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 204 | agree-cex | cex at step 0 reproduced in sim | 0.26 |
| 205 | agree-cex | cex at step 3 reproduced in sim | 0.27 |
| 206 | agree-cex | cex at step 7 reproduced in sim | 0.20 |
| 207 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 208 | agree-cex | cex at step 3 reproduced in sim | 0.26 |
| 209 | agree-quiet | both legs quiet to bound 12 | 0.26 |
| 210 | agree-quiet | both legs quiet to bound 12 | 0.20 |
| 211 | agree-quiet | both legs quiet to bound 12 | 0.20 |
| 212 | agree-cex | cex at step 1 reproduced in sim | 0.28 |
| 213 | agree-cex | cex at step 6 reproduced in sim | 0.29 |
| 214 | agree-quiet | both legs quiet to bound 12 | 0.24 |
| 215 | agree-cex | cex at step 2 reproduced in sim | 0.24 |
| 216 | agree-cex | cex at step 3 reproduced in sim | 0.21 |
| 217 | agree-cex | cex at step 8 reproduced in sim | 0.22 |
| 218 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 219 | agree-cex | cex at step 6 reproduced in sim | 0.23 |
| 220 | agree-cex | cex at step 0 reproduced in sim | 0.23 |
| 221 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 222 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 223 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 224 | agree-cex | cex at step 4 reproduced in sim | 0.24 |
| 225 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 226 | agree-cex | cex at step 6 reproduced in sim | 0.20 |
| 227 | agree-cex | cex at step 0 reproduced in sim | 0.18 |
| 228 | agree-cex | cex at step 0 reproduced in sim | 0.23 |
| 229 | agree-cex | cex at step 4 reproduced in sim | 0.31 |
| 230 | agree-cex | cex at step 4 reproduced in sim | 0.30 |
| 231 | agree-cex | cex at step 3 reproduced in sim | 0.28 |
| 232 | agree-cex | cex at step 2 reproduced in sim | 0.23 |
| 233 | agree-cex | cex at step 0 reproduced in sim | 0.37 |
| 234 | agree-cex | cex at step 3 reproduced in sim | 0.28 |
| 235 | agree-cex | cex at step 7 reproduced in sim | 0.32 |
| 236 | agree-cex | cex at step 0 reproduced in sim | 0.27 |
| 237 | agree-quiet | both legs quiet to bound 12 | 0.45 |
| 238 | agree-cex | cex at step 6 reproduced in sim | 0.28 |
| 239 | agree-cex | cex at step 0 reproduced in sim | 0.32 |
| 240 | agree-cex | cex at step 8 reproduced in sim | 0.26 |
| 241 | agree-cex | cex at step 10 reproduced in sim | 0.28 |
| 242 | agree-cex | cex at step 0 reproduced in sim | 0.23 |
| 243 | agree-cex | cex at step 2 reproduced in sim | 0.23 |
| 244 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 245 | agree-cex | cex at step 1 reproduced in sim | 0.16 |
| 246 | agree-quiet | both legs quiet to bound 12 | 0.17 |
| 247 | agree-cex | cex at step 11 reproduced in sim | 0.26 |
| 248 | agree-quiet | both legs quiet to bound 12 | 0.37 |
| 249 | agree-cex | cex at step 0 reproduced in sim | 0.23 |
| 250 | agree-cex | cex at step 2 reproduced in sim | 0.27 |
| 251 | agree-cex | cex at step 1 reproduced in sim | 0.24 |
| 252 | agree-quiet | both legs quiet to bound 12 | 0.29 |
| 253 | agree-quiet | both legs quiet to bound 12 | 0.28 |
| 254 | agree-cex | cex at step 6 reproduced in sim | 0.27 |
| 255 | agree-cex | cex at step 2 reproduced in sim | 0.21 |
| 256 | agree-quiet | both legs quiet to bound 12 | 0.25 |
| 257 | agree-quiet | both legs quiet to bound 12 | 0.29 |
| 258 | agree-cex | cex at step 0 reproduced in sim | 0.25 |
| 259 | agree-quiet | both legs quiet to bound 12 | 0.27 |
| 260 | agree-cex | cex at step 0 reproduced in sim | 0.26 |
| 261 | agree-cex | cex at step 0 reproduced in sim | 0.26 |
| 262 | agree-quiet | both legs quiet to bound 12 | 0.25 |
| 263 | agree-cex | cex at step 4 reproduced in sim | 0.27 |
| 264 | agree-cex | cex at step 3 reproduced in sim | 0.24 |
| 265 | agree-cex | cex at step 3 reproduced in sim | 0.20 |
| 266 | agree-cex | cex at step 0 reproduced in sim | 0.17 |
| 267 | agree-quiet | both legs quiet to bound 12 | 0.25 |
| 268 | agree-cex | cex at step 0 reproduced in sim | 0.27 |
| 269 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 270 | agree-cex | cex at step 0 reproduced in sim | 0.24 |
| 271 | agree-cex | cex at step 10 reproduced in sim | 0.25 |
| 272 | agree-cex | cex at step 0 reproduced in sim | 0.23 |
| 273 | agree-cex | cex at step 5 reproduced in sim | 0.23 |
| 274 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 275 | agree-cex | cex at step 0 reproduced in sim | 0.27 |
| 276 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 277 | agree-cex | cex at step 7 reproduced in sim | 0.26 |
| 278 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 279 | agree-cex | cex at step 5 reproduced in sim | 0.34 |
| 280 | agree-cex | cex at step 4 reproduced in sim | 0.26 |
| 281 | agree-quiet | both legs quiet to bound 12 | 0.27 |
| 282 | agree-cex | cex at step 0 reproduced in sim | 0.23 |
| 283 | agree-cex | cex at step 8 reproduced in sim | 0.23 |
| 284 | agree-cex | cex at step 1 reproduced in sim | 0.21 |
| 285 | agree-cex | cex at step 2 reproduced in sim | 0.20 |
| 286 | agree-cex | cex at step 4 reproduced in sim | 0.20 |
| 287 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 288 | agree-cex | cex at step 2 reproduced in sim | 0.19 |
| 289 | agree-cex | cex at step 1 reproduced in sim | 0.19 |
| 290 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 291 | agree-cex | cex at step 6 reproduced in sim | 0.29 |
| 292 | agree-cex | cex at step 4 reproduced in sim | 0.23 |
| 293 | agree-cex | cex at step 1 reproduced in sim | 0.24 |
| 294 | agree-quiet | both legs quiet to bound 12 | 0.20 |
| 295 | agree-cex | cex at step 2 reproduced in sim | 0.23 |
| 296 | agree-cex | cex at step 9 reproduced in sim | 0.26 |
| 297 | agree-cex | cex at step 0 reproduced in sim | 0.33 |
| 298 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 299 | agree-quiet | both legs quiet to bound 12 | 0.22 |
| 300 | agree-cex | cex at step 2 reproduced in sim | 0.17 |
| 301 | agree-cex | cex at step 2 reproduced in sim | 0.22 |
| 302 | agree-cex | cex at step 6 reproduced in sim | 0.23 |
| 303 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 304 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 305 | agree-cex | cex at step 3 reproduced in sim | 0.24 |
| 306 | agree-cex | cex at step 3 reproduced in sim | 0.27 |
| 307 | agree-quiet | both legs quiet to bound 12 | 0.27 |
| 308 | agree-cex | cex at step 3 reproduced in sim | 0.22 |
| 309 | agree-cex | cex at step 6 reproduced in sim | 0.30 |
| 310 | agree-cex | cex at step 0 reproduced in sim | 0.26 |
| 311 | agree-cex | cex at step 4 reproduced in sim | 0.24 |
| 312 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 313 | agree-cex | cex at step 1 reproduced in sim | 0.19 |
| 314 | agree-cex | cex at step 2 reproduced in sim | 0.25 |
| 315 | agree-cex | cex at step 11 reproduced in sim | 0.24 |
| 316 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 317 | agree-cex | cex at step 0 reproduced in sim | 0.25 |
| 318 | agree-cex | cex at step 6 reproduced in sim | 0.24 |
| 319 | agree-cex | cex at step 5 reproduced in sim | 0.19 |
| 320 | agree-cex | cex at step 3 reproduced in sim | 0.25 |
| 321 | agree-quiet | both legs quiet to bound 12 | 0.24 |
| 322 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 323 | agree-quiet | both legs quiet to bound 12 | 0.25 |
| 324 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 325 | agree-cex | cex at step 0 reproduced in sim | 0.24 |
| 326 | agree-cex | cex at step 4 reproduced in sim | 0.23 |
| 327 | agree-cex | cex at step 5 reproduced in sim | 0.23 |
| 328 | agree-cex | cex at step 5 reproduced in sim | 0.29 |
| 329 | agree-cex | cex at step 5 reproduced in sim | 0.20 |
| 330 | agree-cex | cex at step 9 reproduced in sim | 0.23 |
| 331 | agree-cex | cex at step 3 reproduced in sim | 0.23 |
| 332 | agree-quiet | both legs quiet to bound 12 | 0.16 |
| 333 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 334 | agree-cex | cex at step 9 reproduced in sim | 0.21 |
| 335 | agree-cex | cex at step 0 reproduced in sim | 0.18 |
| 336 | agree-cex | cex at step 4 reproduced in sim | 0.24 |
| 337 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 338 | agree-quiet | both legs quiet to bound 12 | 0.25 |
| 339 | agree-cex | cex at step 6 reproduced in sim | 0.22 |
| 340 | agree-quiet | both legs quiet to bound 12 | 0.28 |
| 341 | agree-cex | cex at step 0 reproduced in sim | 0.23 |
| 342 | agree-cex | cex at step 4 reproduced in sim | 0.24 |
| 343 | agree-cex | cex at step 1 reproduced in sim | 0.20 |
| 344 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 345 | agree-cex | cex at step 2 reproduced in sim | 0.21 |
| 346 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 347 | agree-cex | cex at step 5 reproduced in sim | 0.20 |
| 348 | agree-cex | cex at step 7 reproduced in sim | 0.22 |
| 349 | agree-cex | cex at step 2 reproduced in sim | 0.25 |
| 350 | agree-quiet | both legs quiet to bound 12 | 0.17 |
| 351 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 352 | agree-quiet | both legs quiet to bound 12 | 0.26 |
| 353 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 354 | agree-cex | cex at step 2 reproduced in sim | 0.19 |
| 355 | agree-cex | cex at step 1 reproduced in sim | 0.26 |
| 356 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 357 | agree-cex | cex at step 1 reproduced in sim | 0.38 |
| 358 | agree-cex | cex at step 5 reproduced in sim | 0.31 |
| 359 | agree-cex | cex at step 3 reproduced in sim | 0.24 |
| 360 | agree-quiet | both legs quiet to bound 12 | 0.20 |
| 361 | agree-cex | cex at step 2 reproduced in sim | 0.30 |
| 362 | agree-cex | cex at step 7 reproduced in sim | 0.24 |
| 363 | agree-cex | cex at step 4 reproduced in sim | 0.26 |
| 364 | agree-quiet | both legs quiet to bound 12 | 0.28 |
| 365 | agree-cex | cex at step 0 reproduced in sim | 0.25 |
| 366 | agree-cex | cex at step 1 reproduced in sim | 0.28 |
| 367 | agree-cex | cex at step 0 reproduced in sim | 0.23 |
| 368 | agree-quiet | both legs quiet to bound 12 | 0.22 |
| 369 | agree-cex | cex at step 3 reproduced in sim | 0.25 |
| 370 | agree-cex | cex at step 2 reproduced in sim | 0.24 |
| 371 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 372 | agree-quiet | both legs quiet to bound 12 | 0.25 |
| 373 | agree-cex | cex at step 2 reproduced in sim | 0.21 |
| 374 | agree-cex | cex at step 3 reproduced in sim | 0.24 |
| 375 | agree-cex | cex at step 6 reproduced in sim | 0.23 |
| 376 | agree-cex | cex at step 3 reproduced in sim | 0.21 |
| 377 | agree-cex | cex at step 1 reproduced in sim | 0.24 |
| 378 | agree-cex | cex at step 0 reproduced in sim | 0.18 |
| 379 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 380 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 381 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 382 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 383 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 384 | agree-cex | cex at step 4 reproduced in sim | 0.27 |
| 385 | agree-quiet | both legs quiet to bound 12 | 0.20 |
| 386 | agree-cex | cex at step 9 reproduced in sim | 0.24 |
| 387 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 388 | agree-cex | cex at step 9 reproduced in sim | 0.28 |
| 389 | agree-cex | cex at step 7 reproduced in sim | 0.24 |
| 390 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 391 | agree-cex | cex at step 7 reproduced in sim | 0.26 |
| 392 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 393 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 394 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 395 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 396 | agree-cex | cex at step 0 reproduced in sim | 0.16 |
| 397 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 398 | agree-cex | cex at step 9 reproduced in sim | 0.22 |
| 399 | agree-quiet | both legs quiet to bound 12 | 0.22 |
| 400 | agree-cex | cex at step 0 reproduced in sim | 0.27 |
| 401 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 402 | agree-cex | cex at step 4 reproduced in sim | 0.22 |
| 403 | agree-cex | cex at step 5 reproduced in sim | 0.24 |
| 404 | agree-quiet | both legs quiet to bound 12 | 0.28 |
| 405 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 406 | agree-cex | cex at step 3 reproduced in sim | 0.22 |
| 407 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 408 | agree-cex | cex at step 2 reproduced in sim | 0.21 |
| 409 | agree-cex | cex at step 0 reproduced in sim | 0.23 |
| 410 | agree-cex | cex at step 3 reproduced in sim | 0.19 |
| 411 | agree-cex | cex at step 10 reproduced in sim | 0.23 |
| 412 | agree-cex | cex at step 2 reproduced in sim | 0.28 |
| 413 | agree-cex | cex at step 6 reproduced in sim | 0.20 |
| 414 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 415 | agree-cex | cex at step 7 reproduced in sim | 0.24 |
| 416 | agree-cex | cex at step 1 reproduced in sim | 0.20 |
| 417 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 418 | agree-cex | cex at step 1 reproduced in sim | 0.23 |
| 419 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 420 | agree-cex | cex at step 1 reproduced in sim | 0.28 |
| 421 | agree-cex | cex at step 6 reproduced in sim | 0.26 |
| 422 | agree-cex | cex at step 3 reproduced in sim | 0.25 |
| 423 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 424 | agree-quiet | both legs quiet to bound 12 | 0.20 |
| 425 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 426 | agree-cex | cex at step 0 reproduced in sim | 0.16 |
| 427 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 428 | agree-cex | cex at step 6 reproduced in sim | 0.24 |
| 429 | agree-cex | cex at step 4 reproduced in sim | 0.23 |
| 430 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 431 | agree-cex | cex at step 2 reproduced in sim | 0.22 |
| 432 | agree-cex | cex at step 10 reproduced in sim | 0.32 |
| 433 | agree-cex | cex at step 6 reproduced in sim | 0.27 |
| 434 | agree-cex | cex at step 10 reproduced in sim | 0.41 |
| 435 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 436 | agree-cex | cex at step 7 reproduced in sim | 0.34 |
| 437 | agree-cex | cex at step 4 reproduced in sim | 0.22 |
| 438 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 439 | agree-cex | cex at step 1 reproduced in sim | 0.25 |
| 440 | agree-cex | cex at step 5 reproduced in sim | 0.25 |
| 441 | agree-cex | cex at step 4 reproduced in sim | 0.21 |
| 442 | agree-cex | cex at step 2 reproduced in sim | 0.19 |
| 443 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 444 | agree-quiet | both legs quiet to bound 12 | 0.36 |
| 445 | agree-cex | cex at step 0 reproduced in sim | 0.23 |
| 446 | agree-cex | cex at step 4 reproduced in sim | 0.30 |
| 447 | agree-cex | cex at step 10 reproduced in sim | 0.29 |
| 448 | agree-quiet | both legs quiet to bound 12 | 0.22 |
| 449 | agree-cex | cex at step 3 reproduced in sim | 0.27 |
| 450 | agree-cex | cex at step 11 reproduced in sim | 0.25 |
| 451 | agree-cex | cex at step 1 reproduced in sim | 0.25 |
| 452 | agree-quiet | both legs quiet to bound 12 | 0.20 |
| 453 | agree-cex | cex at step 0 reproduced in sim | 0.31 |
| 454 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 455 | agree-quiet | both legs quiet to bound 12 | 0.22 |
| 456 | agree-cex | cex at step 0 reproduced in sim | 0.17 |
| 457 | agree-quiet | both legs quiet to bound 12 | 0.17 |
| 458 | agree-cex | cex at step 3 reproduced in sim | 0.23 |
| 459 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 460 | agree-cex | cex at step 10 reproduced in sim | 0.23 |
| 461 | agree-cex | cex at step 4 reproduced in sim | 0.25 |
| 462 | agree-cex | cex at step 2 reproduced in sim | 0.21 |
| 463 | agree-cex | cex at step 10 reproduced in sim | 0.27 |
| 464 | agree-cex | cex at step 5 reproduced in sim | 0.22 |
| 465 | agree-cex | cex at step 1 reproduced in sim | 0.23 |
| 466 | agree-cex | cex at step 4 reproduced in sim | 0.24 |
| 467 | agree-cex | cex at step 0 reproduced in sim | 0.24 |
| 468 | agree-cex | cex at step 3 reproduced in sim | 0.22 |
| 469 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 470 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 471 | agree-cex | cex at step 0 reproduced in sim | 0.26 |
| 472 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 473 | agree-cex | cex at step 0 reproduced in sim | 0.23 |
| 474 | agree-cex | cex at step 0 reproduced in sim | 0.17 |
| 475 | agree-cex | cex at step 7 reproduced in sim | 0.24 |
| 476 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 477 | agree-cex | cex at step 2 reproduced in sim | 0.22 |
| 478 | agree-cex | cex at step 8 reproduced in sim | 0.26 |
| 479 | agree-cex | cex at step 2 reproduced in sim | 0.23 |
| 480 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 481 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 482 | agree-cex | cex at step 6 reproduced in sim | 0.29 |
| 483 | agree-cex | cex at step 8 reproduced in sim | 0.27 |
| 484 | agree-cex | cex at step 0 reproduced in sim | 0.27 |
| 485 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 486 | agree-quiet | both legs quiet to bound 12 | 0.24 |
| 487 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 488 | agree-quiet | both legs quiet to bound 12 | 0.28 |
| 489 | agree-cex | cex at step 8 reproduced in sim | 0.24 |
| 490 | agree-cex | cex at step 1 reproduced in sim | 0.21 |
| 491 | agree-cex | cex at step 1 reproduced in sim | 0.21 |
| 492 | agree-cex | cex at step 1 reproduced in sim | 0.24 |
| 493 | agree-cex | cex at step 8 reproduced in sim | 0.22 |
| 494 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 495 | agree-cex | cex at step 5 reproduced in sim | 0.23 |
| 496 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 497 | agree-cex | cex at step 9 reproduced in sim | 0.25 |
| 498 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 499 | agree-cex | cex at step 0 reproduced in sim | 0.23 |
| 500 | agree-cex | cex at step 0 reproduced in sim | 0.18 |
| 501 | agree-quiet | both legs quiet to bound 12 | 0.24 |
| 502 | agree-quiet | both legs quiet to bound 12 | 0.22 |
| 503 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 504 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 505 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 506 | agree-cex | cex at step 1 reproduced in sim | 0.21 |
| 507 | agree-cex | cex at step 0 reproduced in sim | 0.23 |
| 508 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 509 | agree-cex | cex at step 5 reproduced in sim | 0.27 |
| 510 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 511 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 512 | agree-quiet | both legs quiet to bound 12 | 0.24 |
| 513 | agree-cex | cex at step 11 reproduced in sim | 0.25 |
| 514 | agree-cex | cex at step 8 reproduced in sim | 0.29 |
| 515 | agree-cex | cex at step 4 reproduced in sim | 0.22 |
| 516 | agree-cex | cex at step 3 reproduced in sim | 0.20 |
| 517 | agree-cex | cex at step 1 reproduced in sim | 0.22 |
| 518 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 519 | agree-cex | cex at step 11 reproduced in sim | 0.26 |
| 520 | agree-cex | cex at step 2 reproduced in sim | 0.19 |
| 521 | agree-cex | cex at step 2 reproduced in sim | 0.20 |
| 522 | agree-cex | cex at step 4 reproduced in sim | 0.24 |
| 523 | agree-cex | cex at step 5 reproduced in sim | 0.23 |
| 524 | agree-cex | cex at step 3 reproduced in sim | 0.26 |
| 525 | agree-cex | cex at step 3 reproduced in sim | 0.19 |
| 526 | agree-cex | cex at step 3 reproduced in sim | 0.18 |
| 527 | agree-cex | cex at step 8 reproduced in sim | 0.23 |
| 528 | agree-quiet | both legs quiet to bound 12 | 0.17 |
| 529 | agree-quiet | both legs quiet to bound 12 | 0.25 |
| 530 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 531 | agree-cex | cex at step 0 reproduced in sim | 0.27 |
| 532 | agree-cex | cex at step 4 reproduced in sim | 0.32 |
| 533 | agree-quiet | both legs quiet to bound 12 | 0.22 |
| 534 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 535 | agree-cex | cex at step 0 reproduced in sim | 0.25 |
| 536 | agree-cex | cex at step 7 reproduced in sim | 0.22 |
| 537 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 538 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 539 | agree-cex | cex at step 6 reproduced in sim | 0.21 |
| 540 | agree-cex | cex at step 3 reproduced in sim | 0.22 |
| 541 | agree-cex | cex at step 8 reproduced in sim | 0.26 |
| 542 | agree-cex | cex at step 9 reproduced in sim | 0.21 |
| 543 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 544 | agree-cex | cex at step 4 reproduced in sim | 0.20 |
| 545 | agree-cex | cex at step 3 reproduced in sim | 0.25 |
| 546 | agree-cex | cex at step 3 reproduced in sim | 0.23 |
| 547 | agree-quiet | both legs quiet to bound 12 | 0.25 |
| 548 | agree-cex | cex at step 6 reproduced in sim | 0.27 |
| 549 | agree-cex | cex at step 3 reproduced in sim | 0.24 |
| 550 | agree-cex | cex at step 5 reproduced in sim | 0.20 |
| 551 | agree-quiet | both legs quiet to bound 12 | 0.24 |
| 552 | agree-cex | cex at step 4 reproduced in sim | 0.29 |
| 553 | agree-cex | cex at step 2 reproduced in sim | 0.21 |
| 554 | agree-cex | cex at step 2 reproduced in sim | 0.23 |
| 555 | agree-cex | cex at step 5 reproduced in sim | 0.29 |
| 556 | agree-cex | cex at step 9 reproduced in sim | 0.21 |
| 557 | agree-cex | cex at step 0 reproduced in sim | 0.26 |
| 558 | agree-cex | cex at step 11 reproduced in sim | 0.22 |
| 559 | agree-cex | cex at step 8 reproduced in sim | 0.26 |
| 560 | agree-cex | cex at step 10 reproduced in sim | 0.25 |
| 561 | agree-cex | cex at step 1 reproduced in sim | 0.20 |
| 562 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 563 | agree-cex | cex at step 7 reproduced in sim | 0.23 |
| 564 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 565 | agree-cex | cex at step 2 reproduced in sim | 0.21 |
| 566 | agree-cex | cex at step 3 reproduced in sim | 0.26 |
| 567 | agree-cex | cex at step 6 reproduced in sim | 0.23 |
| 568 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 569 | agree-cex | cex at step 1 reproduced in sim | 0.23 |
| 570 | agree-cex | cex at step 9 reproduced in sim | 0.23 |
| 571 | agree-cex | cex at step 3 reproduced in sim | 0.20 |
| 572 | agree-cex | cex at step 4 reproduced in sim | 0.25 |
| 573 | agree-quiet | both legs quiet to bound 12 | 0.22 |
| 574 | agree-cex | cex at step 0 reproduced in sim | 0.23 |
| 575 | agree-quiet | both legs quiet to bound 12 | 0.29 |
| 576 | agree-cex | cex at step 1 reproduced in sim | 0.20 |
| 577 | agree-cex | cex at step 8 reproduced in sim | 0.30 |
| 578 | agree-cex | cex at step 2 reproduced in sim | 0.23 |
| 579 | agree-cex | cex at step 3 reproduced in sim | 0.21 |
| 580 | agree-cex | cex at step 8 reproduced in sim | 0.24 |
| 581 | agree-cex | cex at step 6 reproduced in sim | 0.17 |
| 582 | agree-cex | cex at step 5 reproduced in sim | 0.18 |
| 583 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 584 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 585 | agree-cex | cex at step 1 reproduced in sim | 0.23 |
| 586 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 587 | agree-cex | cex at step 5 reproduced in sim | 0.24 |
| 588 | agree-cex | cex at step 2 reproduced in sim | 0.21 |
| 589 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 590 | agree-cex | cex at step 3 reproduced in sim | 0.21 |
| 591 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 592 | agree-cex | cex at step 0 reproduced in sim | 0.24 |
| 593 | agree-quiet | both legs quiet to bound 12 | 0.22 |
| 594 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 595 | agree-cex | cex at step 3 reproduced in sim | 0.23 |
| 596 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 597 | agree-cex | cex at step 2 reproduced in sim | 0.19 |
| 598 | agree-cex | cex at step 4 reproduced in sim | 0.20 |
| 599 | agree-cex | cex at step 7 reproduced in sim | 0.19 |
