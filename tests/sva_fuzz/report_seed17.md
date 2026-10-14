# sva_fuzz report -- seed 17, 600 cases, bound 12

- yosys: `/Users/npe/src/sv-yosys/third_party/yosys/build/yosys`
- smtbmc: `/Users/npe/src/sv-yosys/third_party/yosys/build/yosys-smtbmc`, solver z3
- wall time: 6.8 s (0.01 s/case)

## Totals

- cases: 600
- agreement: 600 (quiet 125, cex 475)
- bug-class findings: 0
- determinism guard: PASS (all 600 cases ran the sim leg twice with identical summaries)

## Class C signatures (frontend/lowering gaps, deduped)

- none

## Full per-case log

| index | class | detail | wall s |
|---|---|---|---|
| 0 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 1 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 2 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 3 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 4 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 5 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 6 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 7 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 8 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 9 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 10 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 11 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 12 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 13 | agree-cex | cex at step 10 reproduced in sim | 0.10 |
| 14 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 15 | agree-cex | cex at step 9 reproduced in sim | 0.09 |
| 16 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 17 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 18 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 19 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 20 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 21 | agree-cex | cex at step 6 reproduced in sim | 0.10 |
| 22 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 23 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 24 | agree-cex | cex at step 6 reproduced in sim | 0.10 |
| 25 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 26 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 27 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 28 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 29 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 30 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 31 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 32 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 33 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 34 | agree-cex | cex at step 6 reproduced in sim | 0.08 |
| 35 | agree-cex | cex at step 8 reproduced in sim | 0.10 |
| 36 | agree-cex | cex at step 11 reproduced in sim | 0.10 |
| 37 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 38 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 39 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 40 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 41 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 42 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 43 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 44 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 45 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 46 | agree-cex | cex at step 6 reproduced in sim | 0.08 |
| 47 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 48 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 49 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 50 | agree-cex | cex at step 7 reproduced in sim | 0.08 |
| 51 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 52 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 53 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 54 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 55 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 56 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 57 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 58 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 59 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 60 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 61 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 62 | agree-quiet | both legs quiet to bound 12 | 0.25 |
| 63 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 64 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 65 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 66 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 67 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 68 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 69 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 70 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 71 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 72 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 73 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 74 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 75 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 76 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 77 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 78 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 79 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 80 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 81 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 82 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 83 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 84 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 85 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 86 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 87 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 88 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 89 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 90 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 91 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 92 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 93 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 94 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 95 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 96 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 97 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 98 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 99 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 100 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 101 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 102 | agree-quiet | both legs quiet to bound 12 | 0.12 |
| 103 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 104 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 105 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 106 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 107 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 108 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 109 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 110 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 111 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 112 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 113 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 114 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 115 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 116 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 117 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 118 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 119 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 120 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 121 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 122 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 123 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 124 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 125 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 126 | agree-cex | cex at step 6 reproduced in sim | 0.08 |
| 127 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 128 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 129 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 130 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 131 | agree-cex | cex at step 11 reproduced in sim | 0.09 |
| 132 | agree-cex | cex at step 11 reproduced in sim | 0.09 |
| 133 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 134 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 135 | agree-cex | cex at step 9 reproduced in sim | 0.09 |
| 136 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 137 | agree-cex | cex at step 8 reproduced in sim | 0.08 |
| 138 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 139 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 140 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 141 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 142 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 143 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 144 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 145 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 146 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 147 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 148 | agree-cex | cex at step 9 reproduced in sim | 0.08 |
| 149 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 150 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 151 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 152 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 153 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 154 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 155 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 156 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 157 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 158 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 159 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 160 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 161 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 162 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 163 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 164 | agree-cex | cex at step 10 reproduced in sim | 0.09 |
| 165 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 166 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 167 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 168 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 169 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 170 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 171 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 172 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 173 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 174 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 175 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 176 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 177 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 178 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 179 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 180 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 181 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 182 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 183 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 184 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 185 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 186 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 187 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 188 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 189 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 190 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 191 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 192 | agree-cex | cex at step 11 reproduced in sim | 0.09 |
| 193 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 194 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 195 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 196 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 197 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 198 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 199 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 200 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 201 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 202 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 203 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 204 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 205 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 206 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 207 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 208 | agree-cex | cex at step 8 reproduced in sim | 0.10 |
| 209 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 210 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 211 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 212 | agree-cex | cex at step 4 reproduced in sim | 0.10 |
| 213 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 214 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 215 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 216 | agree-cex | cex at step 9 reproduced in sim | 0.09 |
| 217 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 218 | agree-cex | cex at step 3 reproduced in sim | 0.10 |
| 219 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 220 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 221 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 222 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 223 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 224 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 225 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 226 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 227 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 228 | agree-cex | cex at step 3 reproduced in sim | 0.10 |
| 229 | agree-cex | cex at step 4 reproduced in sim | 0.10 |
| 230 | agree-cex | cex at step 2 reproduced in sim | 0.10 |
| 231 | agree-cex | cex at step 1 reproduced in sim | 0.10 |
| 232 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 233 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 234 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 235 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 236 | agree-cex | cex at step 9 reproduced in sim | 0.09 |
| 237 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 238 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 239 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 240 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 241 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 242 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 243 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 244 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 245 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 246 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 247 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 248 | agree-cex | cex at step 9 reproduced in sim | 0.10 |
| 249 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 250 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 251 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 252 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 253 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 254 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 255 | agree-cex | cex at step 11 reproduced in sim | 0.11 |
| 256 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 257 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 258 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 259 | agree-cex | cex at step 9 reproduced in sim | 0.09 |
| 260 | agree-cex | cex at step 6 reproduced in sim | 0.10 |
| 261 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 262 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 263 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 264 | agree-cex | cex at step 6 reproduced in sim | 0.08 |
| 265 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 266 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 267 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 268 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 269 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 270 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 271 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 272 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 273 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 274 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 275 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 276 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 277 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 278 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 279 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 280 | agree-cex | cex at step 3 reproduced in sim | 0.10 |
| 281 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 282 | agree-cex | cex at step 4 reproduced in sim | 0.11 |
| 283 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 284 | agree-cex | cex at step 3 reproduced in sim | 0.11 |
| 285 | agree-cex | cex at step 3 reproduced in sim | 0.10 |
| 286 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 287 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 288 | agree-cex | cex at step 3 reproduced in sim | 0.11 |
| 289 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 290 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 291 | agree-cex | cex at step 6 reproduced in sim | 0.10 |
| 292 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 293 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 294 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 295 | agree-cex | cex at step 11 reproduced in sim | 0.09 |
| 296 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 297 | agree-quiet | both legs quiet to bound 12 | 0.12 |
| 298 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 299 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 300 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 301 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 302 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 303 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 304 | agree-cex | cex at step 9 reproduced in sim | 0.09 |
| 305 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 306 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 307 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 308 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 309 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 310 | agree-cex | cex at step 9 reproduced in sim | 0.09 |
| 311 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 312 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 313 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 314 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 315 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 316 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 317 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 318 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 319 | agree-cex | cex at step 8 reproduced in sim | 0.10 |
| 320 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 321 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 322 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 323 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 324 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 325 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 326 | agree-cex | cex at step 10 reproduced in sim | 0.10 |
| 327 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 328 | agree-cex | cex at step 6 reproduced in sim | 0.08 |
| 329 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 330 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 331 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 332 | agree-cex | cex at step 10 reproduced in sim | 0.09 |
| 333 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 334 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 335 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 336 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 337 | agree-cex | cex at step 10 reproduced in sim | 0.10 |
| 338 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 339 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 340 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 341 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 342 | agree-cex | cex at step 9 reproduced in sim | 0.09 |
| 343 | agree-cex | cex at step 10 reproduced in sim | 0.08 |
| 344 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 345 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 346 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 347 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 348 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 349 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 350 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 351 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 352 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 353 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 354 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 355 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 356 | agree-cex | cex at step 11 reproduced in sim | 0.10 |
| 357 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 358 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 359 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 360 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 361 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 362 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 363 | agree-cex | cex at step 7 reproduced in sim | 0.08 |
| 364 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 365 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 366 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 367 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 368 | agree-quiet | both legs quiet to bound 12 | 0.16 |
| 369 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 370 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 371 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 372 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 373 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 374 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 375 | agree-cex | cex at step 6 reproduced in sim | 0.10 |
| 376 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 377 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 378 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 379 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 380 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 381 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 382 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 383 | agree-cex | cex at step 11 reproduced in sim | 0.10 |
| 384 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 385 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 386 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 387 | agree-cex | cex at step 9 reproduced in sim | 0.09 |
| 388 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 389 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 390 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 391 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 392 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 393 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 394 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 395 | agree-cex | cex at step 0 reproduced in sim | 0.07 |
| 396 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 397 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 398 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 399 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 400 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 401 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 402 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 403 | agree-cex | cex at step 10 reproduced in sim | 0.10 |
| 404 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 405 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 406 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 407 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 408 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 409 | agree-cex | cex at step 5 reproduced in sim | 0.08 |
| 410 | agree-cex | cex at step 11 reproduced in sim | 0.10 |
| 411 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 412 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 413 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 414 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 415 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 416 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 417 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 418 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 419 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 420 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 421 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 422 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 423 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 424 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 425 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 426 | agree-cex | cex at step 2 reproduced in sim | 0.11 |
| 427 | agree-cex | cex at step 4 reproduced in sim | 0.10 |
| 428 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 429 | agree-cex | cex at step 0 reproduced in sim | 0.11 |
| 430 | agree-cex | cex at step 2 reproduced in sim | 0.11 |
| 431 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 432 | agree-cex | cex at step 9 reproduced in sim | 0.15 |
| 433 | agree-cex | cex at step 5 reproduced in sim | 0.12 |
| 434 | agree-cex | cex at step 2 reproduced in sim | 0.11 |
| 435 | agree-quiet | both legs quiet to bound 12 | 0.12 |
| 436 | agree-cex | cex at step 2 reproduced in sim | 0.13 |
| 437 | agree-cex | cex at step 3 reproduced in sim | 0.12 |
| 438 | agree-cex | cex at step 0 reproduced in sim | 0.12 |
| 439 | agree-quiet | both legs quiet to bound 12 | 0.12 |
| 440 | agree-cex | cex at step 3 reproduced in sim | 0.10 |
| 441 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 442 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 443 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 444 | agree-cex | cex at step 1 reproduced in sim | 0.10 |
| 445 | agree-cex | cex at step 5 reproduced in sim | 0.10 |
| 446 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 447 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 448 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 449 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 450 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 451 | agree-cex | cex at step 3 reproduced in sim | 0.10 |
| 452 | agree-cex | cex at step 0 reproduced in sim | 0.12 |
| 453 | agree-cex | cex at step 5 reproduced in sim | 0.11 |
| 454 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 455 | agree-cex | cex at step 5 reproduced in sim | 0.13 |
| 456 | agree-cex | cex at step 5 reproduced in sim | 0.12 |
| 457 | agree-cex | cex at step 5 reproduced in sim | 0.11 |
| 458 | agree-cex | cex at step 9 reproduced in sim | 0.11 |
| 459 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 460 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 461 | agree-cex | cex at step 2 reproduced in sim | 0.11 |
| 462 | agree-cex | cex at step 1 reproduced in sim | 0.11 |
| 463 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 464 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 465 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 466 | agree-cex | cex at step 3 reproduced in sim | 0.10 |
| 467 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 468 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 469 | agree-cex | cex at step 10 reproduced in sim | 0.09 |
| 470 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 471 | agree-cex | cex at step 2 reproduced in sim | 0.10 |
| 472 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 473 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 474 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 475 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 476 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 477 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 478 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 479 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 480 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 481 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 482 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 483 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 484 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 485 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 486 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 487 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 488 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 489 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 490 | agree-cex | cex at step 9 reproduced in sim | 0.10 |
| 491 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 492 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 493 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 494 | agree-cex | cex at step 7 reproduced in sim | 0.08 |
| 495 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 496 | agree-cex | cex at step 11 reproduced in sim | 0.10 |
| 497 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 498 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 499 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 500 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 501 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 502 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 503 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 504 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 505 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 506 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 507 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 508 | agree-cex | cex at step 9 reproduced in sim | 0.11 |
| 509 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 510 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 511 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 512 | agree-cex | cex at step 7 reproduced in sim | 0.10 |
| 513 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 514 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 515 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 516 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 517 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 518 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 519 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 520 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 521 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 522 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 523 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 524 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 525 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 526 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 527 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 528 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 529 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 530 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 531 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 532 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 533 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 534 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 535 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 536 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 537 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 538 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 539 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 540 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 541 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 542 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 543 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 544 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 545 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 546 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 547 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 548 | agree-cex | cex at step 10 reproduced in sim | 0.10 |
| 549 | agree-cex | cex at step 2 reproduced in sim | 0.10 |
| 550 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 551 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 552 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 553 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 554 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 555 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 556 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 557 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 558 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 559 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 560 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 561 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 562 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 563 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 564 | agree-cex | cex at step 4 reproduced in sim | 0.10 |
| 565 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 566 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 567 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 568 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 569 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 570 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 571 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 572 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 573 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 574 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 575 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 576 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 577 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 578 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 579 | agree-cex | cex at step 3 reproduced in sim | 0.10 |
| 580 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 581 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 582 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 583 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 584 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 585 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 586 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 587 | agree-cex | cex at step 9 reproduced in sim | 0.10 |
| 588 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 589 | agree-cex | cex at step 10 reproduced in sim | 0.11 |
| 590 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 591 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 592 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 593 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 594 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 595 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 596 | agree-cex | cex at step 0 reproduced in sim | 0.07 |
| 597 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 598 | agree-cex | cex at step 0 reproduced in sim | 0.07 |
| 599 | agree-quiet | both legs quiet to bound 12 | 0.07 |
