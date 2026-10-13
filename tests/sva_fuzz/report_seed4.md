# sva_fuzz report -- seed 4, 600 cases, bound 12

- yosys: `/Users/npe/src/sv-yosys/third_party/yosys/build/yosys`
- smtbmc: `/Users/npe/src/sv-yosys/third_party/yosys/build/yosys-smtbmc`, solver z3
- wall time: 17.6 s (0.03 s/case)

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
| 0 | agree-quiet | both legs quiet to bound 12 | 0.22 |
| 1 | agree-cex | cex at step 6 reproduced in sim | 0.26 |
| 2 | agree-cex | cex at step 9 reproduced in sim | 0.28 |
| 3 | agree-cex | cex at step 0 reproduced in sim | 0.29 |
| 4 | agree-cex | cex at step 0 reproduced in sim | 0.27 |
| 5 | agree-cex | cex at step 4 reproduced in sim | 0.28 |
| 6 | agree-quiet | both legs quiet to bound 12 | 0.27 |
| 7 | agree-cex | cex at step 0 reproduced in sim | 0.28 |
| 8 | agree-cex | cex at step 4 reproduced in sim | 0.24 |
| 9 | agree-cex | cex at step 3 reproduced in sim | 0.26 |
| 10 | agree-cex | cex at step 10 reproduced in sim | 0.26 |
| 11 | agree-cex | cex at step 2 reproduced in sim | 0.21 |
| 12 | agree-cex | cex at step 3 reproduced in sim | 0.25 |
| 13 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 14 | agree-cex | cex at step 5 reproduced in sim | 0.25 |
| 15 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 16 | agree-quiet | both legs quiet to bound 12 | 0.25 |
| 17 | agree-cex | cex at step 1 reproduced in sim | 0.25 |
| 18 | agree-cex | cex at step 6 reproduced in sim | 0.25 |
| 19 | agree-cex | cex at step 3 reproduced in sim | 0.22 |
| 20 | agree-cex | cex at step 0 reproduced in sim | 0.24 |
| 21 | agree-quiet | both legs quiet to bound 12 | 0.25 |
| 22 | agree-cex | cex at step 3 reproduced in sim | 0.22 |
| 23 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 24 | agree-cex | cex at step 3 reproduced in sim | 0.20 |
| 25 | agree-cex | cex at step 8 reproduced in sim | 0.25 |
| 26 | agree-cex | cex at step 0 reproduced in sim | 0.28 |
| 27 | agree-cex | cex at step 0 reproduced in sim | 0.28 |
| 28 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 29 | agree-cex | cex at step 3 reproduced in sim | 0.24 |
| 30 | agree-cex | cex at step 0 reproduced in sim | 0.23 |
| 31 | agree-cex | cex at step 4 reproduced in sim | 0.28 |
| 32 | agree-cex | cex at step 3 reproduced in sim | 0.19 |
| 33 | agree-cex | cex at step 1 reproduced in sim | 0.16 |
| 34 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 35 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 36 | agree-cex | cex at step 8 reproduced in sim | 0.25 |
| 37 | agree-cex | cex at step 10 reproduced in sim | 0.23 |
| 38 | agree-cex | cex at step 6 reproduced in sim | 0.25 |
| 39 | agree-quiet | both legs quiet to bound 12 | 0.26 |
| 40 | agree-cex | cex at step 0 reproduced in sim | 0.27 |
| 41 | agree-cex | cex at step 0 reproduced in sim | 0.24 |
| 42 | agree-cex | cex at step 8 reproduced in sim | 0.22 |
| 43 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 44 | agree-cex | cex at step 0 reproduced in sim | 0.25 |
| 45 | agree-cex | cex at step 1 reproduced in sim | 0.21 |
| 46 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 47 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 48 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 49 | agree-cex | cex at step 0 reproduced in sim | 0.23 |
| 50 | agree-cex | cex at step 8 reproduced in sim | 0.26 |
| 51 | agree-quiet | both legs quiet to bound 12 | 0.20 |
| 52 | agree-cex | cex at step 5 reproduced in sim | 0.25 |
| 53 | agree-cex | cex at step 10 reproduced in sim | 0.24 |
| 54 | agree-quiet | both legs quiet to bound 12 | 0.20 |
| 55 | agree-cex | cex at step 4 reproduced in sim | 0.19 |
| 56 | agree-quiet | both legs quiet to bound 12 | 0.20 |
| 57 | agree-cex | cex at step 7 reproduced in sim | 0.25 |
| 58 | agree-cex | cex at step 1 reproduced in sim | 0.20 |
| 59 | agree-cex | cex at step 1 reproduced in sim | 0.22 |
| 60 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 61 | agree-cex | cex at step 3 reproduced in sim | 0.24 |
| 62 | agree-quiet | both legs quiet to bound 12 | 0.31 |
| 63 | agree-cex | cex at step 4 reproduced in sim | 0.27 |
| 64 | agree-cex | cex at step 0 reproduced in sim | 0.29 |
| 65 | agree-cex | cex at step 0 reproduced in sim | 0.30 |
| 66 | agree-cex | cex at step 0 reproduced in sim | 0.25 |
| 67 | agree-cex | cex at step 0 reproduced in sim | 0.26 |
| 68 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 69 | agree-quiet | both legs quiet to bound 12 | 0.27 |
| 70 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 71 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 72 | agree-cex | cex at step 2 reproduced in sim | 0.25 |
| 73 | agree-cex | cex at step 3 reproduced in sim | 0.21 |
| 74 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 75 | agree-cex | cex at step 8 reproduced in sim | 0.24 |
| 76 | agree-cex | cex at step 3 reproduced in sim | 0.19 |
| 77 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 78 | agree-cex | cex at step 8 reproduced in sim | 0.23 |
| 79 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 80 | agree-cex | cex at step 10 reproduced in sim | 0.31 |
| 81 | agree-cex | cex at step 2 reproduced in sim | 0.22 |
| 82 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 83 | agree-cex | cex at step 0 reproduced in sim | 0.25 |
| 84 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 85 | agree-quiet | both legs quiet to bound 12 | 0.16 |
| 86 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 87 | agree-cex | cex at step 0 reproduced in sim | 0.18 |
| 88 | agree-cex | cex at step 7 reproduced in sim | 0.22 |
| 89 | agree-quiet | both legs quiet to bound 12 | 0.16 |
| 90 | agree-quiet | both legs quiet to bound 12 | 0.17 |
| 91 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 92 | agree-cex | cex at step 7 reproduced in sim | 0.20 |
| 93 | agree-cex | cex at step 4 reproduced in sim | 0.21 |
| 94 | agree-cex | cex at step 2 reproduced in sim | 0.19 |
| 95 | agree-cex | cex at step 1 reproduced in sim | 0.23 |
| 96 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 97 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 98 | agree-cex | cex at step 9 reproduced in sim | 0.21 |
| 99 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 100 | agree-cex | cex at step 5 reproduced in sim | 0.33 |
| 101 | agree-cex | cex at step 2 reproduced in sim | 0.23 |
| 102 | agree-cex | cex at step 0 reproduced in sim | 0.23 |
| 103 | agree-cex | cex at step 4 reproduced in sim | 0.23 |
| 104 | agree-cex | cex at step 3 reproduced in sim | 0.32 |
| 105 | agree-cex | cex at step 3 reproduced in sim | 0.27 |
| 106 | agree-quiet | both legs quiet to bound 12 | 0.24 |
| 107 | agree-cex | cex at step 1 reproduced in sim | 0.22 |
| 108 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 109 | agree-quiet | both legs quiet to bound 12 | 0.16 |
| 110 | agree-cex | cex at step 8 reproduced in sim | 0.22 |
| 111 | agree-cex | cex at step 6 reproduced in sim | 0.23 |
| 112 | agree-cex | cex at step 0 reproduced in sim | 0.23 |
| 113 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 114 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 115 | agree-quiet | both legs quiet to bound 12 | 0.20 |
| 116 | agree-quiet | both legs quiet to bound 12 | 0.22 |
| 117 | agree-cex | cex at step 9 reproduced in sim | 0.33 |
| 118 | agree-cex | cex at step 3 reproduced in sim | 0.28 |
| 119 | agree-cex | cex at step 0 reproduced in sim | 0.25 |
| 120 | agree-quiet | both legs quiet to bound 12 | 0.25 |
| 121 | agree-cex | cex at step 2 reproduced in sim | 0.23 |
| 122 | agree-cex | cex at step 5 reproduced in sim | 0.30 |
| 123 | agree-cex | cex at step 0 reproduced in sim | 0.27 |
| 124 | agree-cex | cex at step 0 reproduced in sim | 0.29 |
| 125 | agree-cex | cex at step 0 reproduced in sim | 0.18 |
| 126 | agree-cex | cex at step 8 reproduced in sim | 0.20 |
| 127 | agree-quiet | both legs quiet to bound 12 | 0.20 |
| 128 | agree-cex | cex at step 0 reproduced in sim | 0.17 |
| 129 | agree-quiet | both legs quiet to bound 12 | 0.26 |
| 130 | agree-cex | cex at step 0 reproduced in sim | 0.18 |
| 131 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 132 | agree-cex | cex at step 3 reproduced in sim | 0.20 |
| 133 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 134 | agree-cex | cex at step 1 reproduced in sim | 0.22 |
| 135 | agree-quiet | both legs quiet to bound 12 | 0.17 |
| 136 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 137 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 138 | agree-cex | cex at step 10 reproduced in sim | 0.26 |
| 139 | agree-cex | cex at step 1 reproduced in sim | 0.18 |
| 140 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 141 | agree-cex | cex at step 5 reproduced in sim | 0.23 |
| 142 | agree-quiet | both legs quiet to bound 12 | 0.24 |
| 143 | agree-cex | cex at step 5 reproduced in sim | 0.21 |
| 144 | agree-quiet | both legs quiet to bound 12 | 0.20 |
| 145 | agree-cex | cex at step 3 reproduced in sim | 0.21 |
| 146 | agree-quiet | both legs quiet to bound 12 | 0.34 |
| 147 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 148 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 149 | agree-cex | cex at step 2 reproduced in sim | 0.19 |
| 150 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 151 | agree-quiet | both legs quiet to bound 12 | 0.22 |
| 152 | agree-cex | cex at step 9 reproduced in sim | 0.28 |
| 153 | agree-cex | cex at step 3 reproduced in sim | 0.24 |
| 154 | agree-cex | cex at step 2 reproduced in sim | 0.17 |
| 155 | agree-cex | cex at step 10 reproduced in sim | 0.23 |
| 156 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 157 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 158 | agree-cex | cex at step 2 reproduced in sim | 0.23 |
| 159 | agree-cex | cex at step 0 reproduced in sim | 0.17 |
| 160 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 161 | agree-cex | cex at step 2 reproduced in sim | 0.22 |
| 162 | agree-quiet | both legs quiet to bound 12 | 0.20 |
| 163 | agree-cex | cex at step 3 reproduced in sim | 0.18 |
| 164 | agree-cex | cex at step 0 reproduced in sim | 0.18 |
| 165 | agree-cex | cex at step 2 reproduced in sim | 0.22 |
| 166 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 167 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 168 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 169 | agree-cex | cex at step 3 reproduced in sim | 0.23 |
| 170 | agree-cex | cex at step 5 reproduced in sim | 0.20 |
| 171 | agree-quiet | both legs quiet to bound 12 | 0.20 |
| 172 | agree-cex | cex at step 4 reproduced in sim | 0.23 |
| 173 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 174 | agree-cex | cex at step 6 reproduced in sim | 0.20 |
| 175 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 176 | agree-cex | cex at step 8 reproduced in sim | 0.22 |
| 177 | agree-cex | cex at step 6 reproduced in sim | 0.20 |
| 178 | agree-cex | cex at step 1 reproduced in sim | 0.18 |
| 179 | agree-cex | cex at step 0 reproduced in sim | 0.16 |
| 180 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 181 | agree-cex | cex at step 8 reproduced in sim | 0.23 |
| 182 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 183 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 184 | agree-cex | cex at step 0 reproduced in sim | 0.25 |
| 185 | agree-quiet | both legs quiet to bound 12 | 0.24 |
| 186 | agree-cex | cex at step 8 reproduced in sim | 0.27 |
| 187 | agree-cex | cex at step 4 reproduced in sim | 0.29 |
| 188 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 189 | agree-cex | cex at step 8 reproduced in sim | 0.27 |
| 190 | agree-cex | cex at step 5 reproduced in sim | 0.20 |
| 191 | agree-cex | cex at step 4 reproduced in sim | 0.26 |
| 192 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 193 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 194 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 195 | agree-cex | cex at step 0 reproduced in sim | 0.23 |
| 196 | agree-cex | cex at step 6 reproduced in sim | 0.29 |
| 197 | agree-cex | cex at step 0 reproduced in sim | 0.23 |
| 198 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 199 | agree-cex | cex at step 6 reproduced in sim | 0.23 |
| 200 | agree-cex | cex at step 2 reproduced in sim | 0.24 |
| 201 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 202 | agree-cex | cex at step 6 reproduced in sim | 0.19 |
| 203 | agree-quiet | both legs quiet to bound 12 | 0.22 |
| 204 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 205 | agree-cex | cex at step 11 reproduced in sim | 0.24 |
| 206 | agree-cex | cex at step 2 reproduced in sim | 0.22 |
| 207 | agree-cex | cex at step 5 reproduced in sim | 0.21 |
| 208 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 209 | agree-cex | cex at step 9 reproduced in sim | 0.17 |
| 210 | agree-quiet | both legs quiet to bound 12 | 0.15 |
| 211 | agree-cex | cex at step 4 reproduced in sim | 0.20 |
| 212 | agree-cex | cex at step 11 reproduced in sim | 0.22 |
| 213 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 214 | agree-cex | cex at step 2 reproduced in sim | 0.21 |
| 215 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 216 | agree-cex | cex at step 2 reproduced in sim | 0.23 |
| 217 | agree-cex | cex at step 3 reproduced in sim | 0.22 |
| 218 | agree-cex | cex at step 9 reproduced in sim | 0.27 |
| 219 | agree-quiet | both legs quiet to bound 12 | 0.16 |
| 220 | agree-cex | cex at step 0 reproduced in sim | 0.23 |
| 221 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 222 | agree-cex | cex at step 4 reproduced in sim | 0.24 |
| 223 | agree-cex | cex at step 0 reproduced in sim | 0.24 |
| 224 | agree-cex | cex at step 11 reproduced in sim | 0.24 |
| 225 | agree-cex | cex at step 6 reproduced in sim | 0.21 |
| 226 | agree-quiet | both legs quiet to bound 12 | 0.24 |
| 227 | agree-cex | cex at step 6 reproduced in sim | 0.19 |
| 228 | agree-quiet | both legs quiet to bound 12 | 0.28 |
| 229 | agree-cex | cex at step 5 reproduced in sim | 0.19 |
| 230 | agree-cex | cex at step 3 reproduced in sim | 0.26 |
| 231 | agree-cex | cex at step 6 reproduced in sim | 0.25 |
| 232 | agree-cex | cex at step 2 reproduced in sim | 0.24 |
| 233 | agree-quiet | both legs quiet to bound 12 | 0.29 |
| 234 | agree-cex | cex at step 4 reproduced in sim | 0.28 |
| 235 | agree-cex | cex at step 3 reproduced in sim | 0.25 |
| 236 | agree-cex | cex at step 6 reproduced in sim | 0.28 |
| 237 | agree-cex | cex at step 4 reproduced in sim | 0.22 |
| 238 | agree-quiet | both legs quiet to bound 12 | 0.31 |
| 239 | agree-quiet | both legs quiet to bound 12 | 0.24 |
| 240 | agree-cex | cex at step 6 reproduced in sim | 0.23 |
| 241 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 242 | agree-quiet | both legs quiet to bound 12 | 0.22 |
| 243 | agree-cex | cex at step 0 reproduced in sim | 0.16 |
| 244 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 245 | agree-cex | cex at step 6 reproduced in sim | 0.20 |
| 246 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 247 | agree-cex | cex at step 1 reproduced in sim | 0.18 |
| 248 | agree-cex | cex at step 0 reproduced in sim | 0.18 |
| 249 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 250 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 251 | agree-cex | cex at step 6 reproduced in sim | 0.23 |
| 252 | agree-cex | cex at step 8 reproduced in sim | 0.25 |
| 253 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 254 | agree-cex | cex at step 0 reproduced in sim | 0.25 |
| 255 | agree-cex | cex at step 9 reproduced in sim | 0.27 |
| 256 | agree-cex | cex at step 5 reproduced in sim | 0.25 |
| 257 | agree-cex | cex at step 5 reproduced in sim | 0.22 |
| 258 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 259 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 260 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 261 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 262 | agree-cex | cex at step 7 reproduced in sim | 0.26 |
| 263 | agree-cex | cex at step 4 reproduced in sim | 0.29 |
| 264 | agree-cex | cex at step 3 reproduced in sim | 0.20 |
| 265 | agree-cex | cex at step 7 reproduced in sim | 0.23 |
| 266 | agree-cex | cex at step 2 reproduced in sim | 0.23 |
| 267 | agree-cex | cex at step 0 reproduced in sim | 0.23 |
| 268 | agree-quiet | both legs quiet to bound 12 | 0.20 |
| 269 | agree-cex | cex at step 4 reproduced in sim | 0.21 |
| 270 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 271 | agree-quiet | both legs quiet to bound 12 | 0.20 |
| 272 | agree-cex | cex at step 1 reproduced in sim | 0.19 |
| 273 | agree-cex | cex at step 0 reproduced in sim | 0.27 |
| 274 | agree-cex | cex at step 3 reproduced in sim | 0.29 |
| 275 | agree-cex | cex at step 3 reproduced in sim | 0.21 |
| 276 | agree-cex | cex at step 0 reproduced in sim | 0.25 |
| 277 | agree-cex | cex at step 3 reproduced in sim | 0.20 |
| 278 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 279 | agree-cex | cex at step 1 reproduced in sim | 0.21 |
| 280 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 281 | agree-cex | cex at step 4 reproduced in sim | 0.20 |
| 282 | agree-cex | cex at step 2 reproduced in sim | 0.23 |
| 283 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 284 | agree-cex | cex at step 2 reproduced in sim | 0.18 |
| 285 | agree-quiet | both legs quiet to bound 12 | 0.36 |
| 286 | agree-cex | cex at step 9 reproduced in sim | 0.31 |
| 287 | agree-quiet | both legs quiet to bound 12 | 0.27 |
| 288 | agree-cex | cex at step 4 reproduced in sim | 0.24 |
| 289 | agree-quiet | both legs quiet to bound 12 | 0.26 |
| 290 | agree-cex | cex at step 5 reproduced in sim | 0.21 |
| 291 | agree-cex | cex at step 5 reproduced in sim | 0.24 |
| 292 | agree-cex | cex at step 10 reproduced in sim | 0.24 |
| 293 | agree-quiet | both legs quiet to bound 12 | 0.16 |
| 294 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 295 | agree-cex | cex at step 7 reproduced in sim | 0.27 |
| 296 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 297 | agree-cex | cex at step 0 reproduced in sim | 0.28 |
| 298 | agree-cex | cex at step 0 reproduced in sim | 0.23 |
| 299 | agree-cex | cex at step 2 reproduced in sim | 0.22 |
| 300 | agree-cex | cex at step 4 reproduced in sim | 0.22 |
| 301 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 302 | agree-quiet | both legs quiet to bound 12 | 0.60 |
| 303 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 304 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 305 | agree-cex | cex at step 3 reproduced in sim | 0.23 |
| 306 | agree-quiet | both legs quiet to bound 12 | 0.17 |
| 307 | agree-cex | cex at step 3 reproduced in sim | 0.27 |
| 308 | agree-cex | cex at step 6 reproduced in sim | 0.27 |
| 309 | agree-cex | cex at step 6 reproduced in sim | 0.23 |
| 310 | agree-cex | cex at step 0 reproduced in sim | 0.16 |
| 311 | agree-quiet | both legs quiet to bound 12 | 0.16 |
| 312 | agree-cex | cex at step 5 reproduced in sim | 0.21 |
| 313 | agree-cex | cex at step 5 reproduced in sim | 0.19 |
| 314 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 315 | agree-cex | cex at step 7 reproduced in sim | 0.25 |
| 316 | agree-quiet | both legs quiet to bound 12 | 0.20 |
| 317 | agree-cex | cex at step 2 reproduced in sim | 0.21 |
| 318 | agree-cex | cex at step 3 reproduced in sim | 0.20 |
| 319 | agree-quiet | both legs quiet to bound 12 | 0.16 |
| 320 | agree-cex | cex at step 2 reproduced in sim | 0.16 |
| 321 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 322 | agree-cex | cex at step 1 reproduced in sim | 0.20 |
| 323 | agree-quiet | both legs quiet to bound 12 | 0.22 |
| 324 | agree-quiet | both legs quiet to bound 12 | 0.20 |
| 325 | agree-cex | cex at step 0 reproduced in sim | 0.25 |
| 326 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 327 | agree-cex | cex at step 6 reproduced in sim | 0.29 |
| 328 | agree-cex | cex at step 7 reproduced in sim | 0.22 |
| 329 | agree-cex | cex at step 0 reproduced in sim | 0.18 |
| 330 | agree-cex | cex at step 0 reproduced in sim | 0.24 |
| 331 | agree-cex | cex at step 5 reproduced in sim | 0.20 |
| 332 | agree-quiet | both legs quiet to bound 12 | 0.24 |
| 333 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 334 | agree-cex | cex at step 8 reproduced in sim | 0.20 |
| 335 | agree-cex | cex at step 2 reproduced in sim | 0.23 |
| 336 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 337 | agree-cex | cex at step 10 reproduced in sim | 0.33 |
| 338 | agree-cex | cex at step 5 reproduced in sim | 0.24 |
| 339 | agree-cex | cex at step 2 reproduced in sim | 0.21 |
| 340 | agree-cex | cex at step 2 reproduced in sim | 0.23 |
| 341 | agree-cex | cex at step 5 reproduced in sim | 0.22 |
| 342 | agree-cex | cex at step 1 reproduced in sim | 0.21 |
| 343 | agree-cex | cex at step 2 reproduced in sim | 0.17 |
| 344 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 345 | agree-cex | cex at step 9 reproduced in sim | 0.21 |
| 346 | agree-cex | cex at step 3 reproduced in sim | 0.23 |
| 347 | agree-cex | cex at step 4 reproduced in sim | 0.28 |
| 348 | agree-cex | cex at step 4 reproduced in sim | 0.21 |
| 349 | agree-cex | cex at step 0 reproduced in sim | 0.24 |
| 350 | agree-cex | cex at step 4 reproduced in sim | 0.27 |
| 351 | agree-quiet | both legs quiet to bound 12 | 0.20 |
| 352 | agree-cex | cex at step 7 reproduced in sim | 0.20 |
| 353 | agree-cex | cex at step 2 reproduced in sim | 0.22 |
| 354 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 355 | agree-cex | cex at step 5 reproduced in sim | 0.30 |
| 356 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 357 | agree-cex | cex at step 0 reproduced in sim | 0.24 |
| 358 | agree-cex | cex at step 0 reproduced in sim | 0.25 |
| 359 | agree-cex | cex at step 2 reproduced in sim | 0.24 |
| 360 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 361 | agree-cex | cex at step 4 reproduced in sim | 0.19 |
| 362 | agree-cex | cex at step 1 reproduced in sim | 0.23 |
| 363 | agree-cex | cex at step 5 reproduced in sim | 0.20 |
| 364 | agree-cex | cex at step 7 reproduced in sim | 0.23 |
| 365 | agree-cex | cex at step 10 reproduced in sim | 0.23 |
| 366 | agree-quiet | both legs quiet to bound 12 | 0.22 |
| 367 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 368 | agree-cex | cex at step 2 reproduced in sim | 0.23 |
| 369 | agree-cex | cex at step 10 reproduced in sim | 0.23 |
| 370 | agree-cex | cex at step 7 reproduced in sim | 0.22 |
| 371 | agree-cex | cex at step 2 reproduced in sim | 0.16 |
| 372 | agree-cex | cex at step 0 reproduced in sim | 0.26 |
| 373 | agree-cex | cex at step 9 reproduced in sim | 0.24 |
| 374 | agree-cex | cex at step 0 reproduced in sim | 0.25 |
| 375 | agree-cex | cex at step 5 reproduced in sim | 0.35 |
| 376 | agree-cex | cex at step 2 reproduced in sim | 0.25 |
| 377 | agree-cex | cex at step 6 reproduced in sim | 0.23 |
| 378 | agree-cex | cex at step 2 reproduced in sim | 0.24 |
| 379 | agree-cex | cex at step 1 reproduced in sim | 0.21 |
| 380 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 381 | agree-cex | cex at step 4 reproduced in sim | 0.27 |
| 382 | agree-quiet | both legs quiet to bound 12 | 0.24 |
| 383 | agree-cex | cex at step 1 reproduced in sim | 0.24 |
| 384 | agree-cex | cex at step 6 reproduced in sim | 0.23 |
| 385 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 386 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 387 | agree-cex | cex at step 3 reproduced in sim | 0.21 |
| 388 | agree-cex | cex at step 0 reproduced in sim | 0.17 |
| 389 | agree-cex | cex at step 1 reproduced in sim | 0.17 |
| 390 | agree-cex | cex at step 6 reproduced in sim | 0.17 |
| 391 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 392 | agree-quiet | both legs quiet to bound 12 | 0.16 |
| 393 | agree-cex | cex at step 8 reproduced in sim | 0.20 |
| 394 | agree-cex | cex at step 7 reproduced in sim | 0.22 |
| 395 | agree-quiet | both legs quiet to bound 12 | 0.27 |
| 396 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 397 | agree-quiet | both legs quiet to bound 12 | 0.30 |
| 398 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 399 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 400 | agree-cex | cex at step 1 reproduced in sim | 0.19 |
| 401 | agree-cex | cex at step 4 reproduced in sim | 0.23 |
| 402 | agree-cex | cex at step 1 reproduced in sim | 0.23 |
| 403 | agree-cex | cex at step 7 reproduced in sim | 0.18 |
| 404 | agree-cex | cex at step 2 reproduced in sim | 0.19 |
| 405 | agree-quiet | both legs quiet to bound 12 | 0.40 |
| 406 | agree-cex | cex at step 4 reproduced in sim | 0.18 |
| 407 | agree-cex | cex at step 3 reproduced in sim | 0.19 |
| 408 | agree-cex | cex at step 11 reproduced in sim | 0.19 |
| 409 | agree-cex | cex at step 0 reproduced in sim | 0.18 |
| 410 | agree-cex | cex at step 4 reproduced in sim | 0.17 |
| 411 | agree-cex | cex at step 9 reproduced in sim | 0.23 |
| 412 | agree-quiet | both legs quiet to bound 12 | 0.16 |
| 413 | agree-cex | cex at step 3 reproduced in sim | 0.28 |
| 414 | agree-cex | cex at step 1 reproduced in sim | 0.24 |
| 415 | agree-cex | cex at step 4 reproduced in sim | 0.22 |
| 416 | agree-cex | cex at step 0 reproduced in sim | 0.23 |
| 417 | agree-quiet | both legs quiet to bound 12 | 0.20 |
| 418 | agree-cex | cex at step 2 reproduced in sim | 0.21 |
| 419 | agree-cex | cex at step 0 reproduced in sim | 0.23 |
| 420 | agree-cex | cex at step 0 reproduced in sim | 0.17 |
| 421 | agree-quiet | both legs quiet to bound 12 | 0.16 |
| 422 | agree-cex | cex at step 2 reproduced in sim | 0.20 |
| 423 | agree-cex | cex at step 11 reproduced in sim | 0.21 |
| 424 | agree-cex | cex at step 5 reproduced in sim | 0.20 |
| 425 | agree-cex | cex at step 2 reproduced in sim | 0.22 |
| 426 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 427 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 428 | agree-cex | cex at step 2 reproduced in sim | 0.21 |
| 429 | agree-quiet | both legs quiet to bound 12 | 0.22 |
| 430 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 431 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 432 | agree-quiet | both legs quiet to bound 12 | 0.16 |
| 433 | agree-cex | cex at step 3 reproduced in sim | 0.19 |
| 434 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 435 | agree-cex | cex at step 6 reproduced in sim | 0.26 |
| 436 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 437 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 438 | agree-cex | cex at step 7 reproduced in sim | 0.22 |
| 439 | agree-cex | cex at step 10 reproduced in sim | 0.29 |
| 440 | agree-cex | cex at step 1 reproduced in sim | 0.23 |
| 441 | agree-cex | cex at step 4 reproduced in sim | 0.21 |
| 442 | agree-quiet | both legs quiet to bound 12 | 0.24 |
| 443 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 444 | agree-cex | cex at step 2 reproduced in sim | 0.23 |
| 445 | agree-cex | cex at step 3 reproduced in sim | 0.26 |
| 446 | agree-cex | cex at step 0 reproduced in sim | 0.23 |
| 447 | agree-cex | cex at step 3 reproduced in sim | 0.26 |
| 448 | agree-cex | cex at step 0 reproduced in sim | 0.26 |
| 449 | agree-cex | cex at step 3 reproduced in sim | 0.23 |
| 450 | agree-cex | cex at step 0 reproduced in sim | 0.17 |
| 451 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 452 | agree-cex | cex at step 1 reproduced in sim | 0.24 |
| 453 | agree-cex | cex at step 8 reproduced in sim | 0.20 |
| 454 | agree-quiet | both legs quiet to bound 12 | 0.22 |
| 455 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 456 | agree-cex | cex at step 2 reproduced in sim | 0.23 |
| 457 | agree-cex | cex at step 4 reproduced in sim | 0.31 |
| 458 | agree-cex | cex at step 2 reproduced in sim | 0.27 |
| 459 | agree-cex | cex at step 0 reproduced in sim | 0.29 |
| 460 | agree-cex | cex at step 7 reproduced in sim | 0.29 |
| 461 | agree-quiet | both legs quiet to bound 12 | 0.24 |
| 462 | agree-cex | cex at step 0 reproduced in sim | 0.27 |
| 463 | agree-cex | cex at step 4 reproduced in sim | 0.25 |
| 464 | agree-cex | cex at step 7 reproduced in sim | 0.25 |
| 465 | agree-quiet | both legs quiet to bound 12 | 0.20 |
| 466 | agree-cex | cex at step 1 reproduced in sim | 0.24 |
| 467 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 468 | agree-quiet | both legs quiet to bound 12 | 0.20 |
| 469 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 470 | agree-quiet | both legs quiet to bound 12 | 0.24 |
| 471 | agree-cex | cex at step 1 reproduced in sim | 0.17 |
| 472 | agree-cex | cex at step 0 reproduced in sim | 0.23 |
| 473 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 474 | agree-quiet | both legs quiet to bound 12 | 0.24 |
| 475 | agree-cex | cex at step 0 reproduced in sim | 0.27 |
| 476 | agree-cex | cex at step 6 reproduced in sim | 0.20 |
| 477 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 478 | agree-quiet | both legs quiet to bound 12 | 0.24 |
| 479 | agree-cex | cex at step 11 reproduced in sim | 0.23 |
| 480 | agree-quiet | both legs quiet to bound 12 | 0.24 |
| 481 | agree-cex | cex at step 2 reproduced in sim | 0.22 |
| 482 | agree-quiet | both legs quiet to bound 12 | 0.20 |
| 483 | agree-cex | cex at step 9 reproduced in sim | 0.33 |
| 484 | agree-cex | cex at step 0 reproduced in sim | 0.23 |
| 485 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 486 | agree-quiet | both legs quiet to bound 12 | 0.25 |
| 487 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 488 | agree-quiet | both legs quiet to bound 12 | 0.28 |
| 489 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 490 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 491 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 492 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 493 | agree-cex | cex at step 11 reproduced in sim | 0.28 |
| 494 | agree-cex | cex at step 9 reproduced in sim | 0.26 |
| 495 | agree-quiet | both legs quiet to bound 12 | 0.24 |
| 496 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 497 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 498 | agree-cex | cex at step 0 reproduced in sim | 0.16 |
| 499 | agree-cex | cex at step 3 reproduced in sim | 0.24 |
| 500 | agree-cex | cex at step 2 reproduced in sim | 0.19 |
| 501 | agree-cex | cex at step 10 reproduced in sim | 0.26 |
| 502 | agree-cex | cex at step 0 reproduced in sim | 0.18 |
| 503 | agree-quiet | both legs quiet to bound 12 | 0.25 |
| 504 | agree-cex | cex at step 8 reproduced in sim | 0.20 |
| 505 | agree-cex | cex at step 0 reproduced in sim | 0.28 |
| 506 | agree-cex | cex at step 4 reproduced in sim | 0.22 |
| 507 | agree-quiet | both legs quiet to bound 12 | 0.18 |
| 508 | agree-cex | cex at step 3 reproduced in sim | 0.18 |
| 509 | agree-cex | cex at step 7 reproduced in sim | 0.19 |
| 510 | agree-cex | cex at step 0 reproduced in sim | 0.24 |
| 511 | agree-quiet | both legs quiet to bound 12 | 0.25 |
| 512 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 513 | agree-cex | cex at step 9 reproduced in sim | 0.29 |
| 514 | agree-cex | cex at step 0 reproduced in sim | 0.29 |
| 515 | agree-cex | cex at step 6 reproduced in sim | 0.32 |
| 516 | agree-cex | cex at step 3 reproduced in sim | 0.25 |
| 517 | agree-cex | cex at step 0 reproduced in sim | 0.26 |
| 518 | agree-cex | cex at step 4 reproduced in sim | 0.22 |
| 519 | agree-cex | cex at step 6 reproduced in sim | 0.24 |
| 520 | agree-cex | cex at step 7 reproduced in sim | 0.32 |
| 521 | agree-cex | cex at step 3 reproduced in sim | 0.31 |
| 522 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 523 | agree-quiet | both legs quiet to bound 12 | 0.22 |
| 524 | agree-quiet | both legs quiet to bound 12 | 0.20 |
| 525 | agree-cex | cex at step 3 reproduced in sim | 0.30 |
| 526 | agree-cex | cex at step 2 reproduced in sim | 0.21 |
| 527 | agree-cex | cex at step 4 reproduced in sim | 0.23 |
| 528 | agree-quiet | both legs quiet to bound 12 | 0.16 |
| 529 | agree-cex | cex at step 8 reproduced in sim | 0.31 |
| 530 | agree-quiet | both legs quiet to bound 12 | 0.27 |
| 531 | agree-cex | cex at step 3 reproduced in sim | 0.17 |
| 532 | agree-cex | cex at step 10 reproduced in sim | 0.21 |
| 533 | agree-cex | cex at step 10 reproduced in sim | 0.21 |
| 534 | agree-cex | cex at step 6 reproduced in sim | 0.17 |
| 535 | agree-cex | cex at step 0 reproduced in sim | 0.14 |
| 536 | agree-cex | cex at step 11 reproduced in sim | 0.27 |
| 537 | agree-quiet | both legs quiet to bound 12 | 0.22 |
| 538 | agree-cex | cex at step 6 reproduced in sim | 0.17 |
| 539 | agree-cex | cex at step 9 reproduced in sim | 0.23 |
| 540 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 541 | agree-cex | cex at step 2 reproduced in sim | 0.23 |
| 542 | agree-cex | cex at step 5 reproduced in sim | 0.22 |
| 543 | agree-quiet | both legs quiet to bound 12 | 0.16 |
| 544 | agree-cex | cex at step 0 reproduced in sim | 0.18 |
| 545 | agree-cex | cex at step 1 reproduced in sim | 0.23 |
| 546 | agree-cex | cex at step 0 reproduced in sim | 0.20 |
| 547 | agree-cex | cex at step 0 reproduced in sim | 0.25 |
| 548 | agree-cex | cex at step 0 reproduced in sim | 0.24 |
| 549 | agree-cex | cex at step 10 reproduced in sim | 0.32 |
| 550 | agree-cex | cex at step 1 reproduced in sim | 0.22 |
| 551 | agree-cex | cex at step 9 reproduced in sim | 0.29 |
| 552 | agree-cex | cex at step 3 reproduced in sim | 0.28 |
| 553 | agree-quiet | both legs quiet to bound 12 | 0.24 |
| 554 | agree-cex | cex at step 0 reproduced in sim | 0.29 |
| 555 | agree-cex | cex at step 0 reproduced in sim | 0.23 |
| 556 | agree-cex | cex at step 0 reproduced in sim | 0.24 |
| 557 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 558 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 559 | agree-quiet | both legs quiet to bound 12 | 0.20 |
| 560 | agree-cex | cex at step 3 reproduced in sim | 0.25 |
| 561 | agree-cex | cex at step 2 reproduced in sim | 0.21 |
| 562 | agree-cex | cex at step 2 reproduced in sim | 0.22 |
| 563 | agree-cex | cex at step 8 reproduced in sim | 0.25 |
| 564 | agree-cex | cex at step 0 reproduced in sim | 0.24 |
| 565 | agree-cex | cex at step 4 reproduced in sim | 0.24 |
| 566 | agree-cex | cex at step 6 reproduced in sim | 0.25 |
| 567 | agree-cex | cex at step 3 reproduced in sim | 0.21 |
| 568 | agree-cex | cex at step 3 reproduced in sim | 0.20 |
| 569 | agree-cex | cex at step 1 reproduced in sim | 0.21 |
| 570 | agree-cex | cex at step 3 reproduced in sim | 0.25 |
| 571 | agree-cex | cex at step 2 reproduced in sim | 0.20 |
| 572 | agree-quiet | both legs quiet to bound 12 | 0.23 |
| 573 | agree-cex | cex at step 5 reproduced in sim | 0.21 |
| 574 | agree-cex | cex at step 5 reproduced in sim | 0.31 |
| 575 | agree-cex | cex at step 0 reproduced in sim | 0.18 |
| 576 | agree-cex | cex at step 0 reproduced in sim | 0.25 |
| 577 | agree-cex | cex at step 3 reproduced in sim | 0.22 |
| 578 | agree-quiet | both legs quiet to bound 12 | 0.33 |
| 579 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 580 | agree-cex | cex at step 7 reproduced in sim | 0.25 |
| 581 | agree-cex | cex at step 3 reproduced in sim | 0.30 |
| 582 | agree-cex | cex at step 6 reproduced in sim | 0.24 |
| 583 | agree-cex | cex at step 9 reproduced in sim | 0.23 |
| 584 | agree-quiet | both legs quiet to bound 12 | 0.21 |
| 585 | agree-cex | cex at step 0 reproduced in sim | 0.21 |
| 586 | agree-quiet | both legs quiet to bound 12 | 0.20 |
| 587 | agree-cex | cex at step 0 reproduced in sim | 0.18 |
| 588 | agree-quiet | both legs quiet to bound 12 | 0.17 |
| 589 | agree-cex | cex at step 0 reproduced in sim | 0.19 |
| 590 | agree-cex | cex at step 6 reproduced in sim | 0.30 |
| 591 | agree-cex | cex at step 3 reproduced in sim | 0.25 |
| 592 | agree-cex | cex at step 1 reproduced in sim | 0.22 |
| 593 | agree-quiet | both legs quiet to bound 12 | 0.20 |
| 594 | agree-cex | cex at step 0 reproduced in sim | 0.22 |
| 595 | agree-cex | cex at step 8 reproduced in sim | 0.21 |
| 596 | agree-quiet | both legs quiet to bound 12 | 0.19 |
| 597 | agree-cex | cex at step 6 reproduced in sim | 0.17 |
| 598 | agree-cex | cex at step 0 reproduced in sim | 0.17 |
| 599 | agree-cex | cex at step 0 reproduced in sim | 0.17 |
