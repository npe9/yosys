# sva_fuzz report -- seed 1, 300 cases, bound 12

- yosys: `/Users/npe/src/sv-yosys/third_party/yosys/build/yosys`
- smtbmc: `/Users/npe/src/sv-yosys/third_party/yosys/build/yosys-smtbmc`, solver z3
- wall time: 3.5 s (0.01 s/case)

## Totals

- cases: 300
- agreement: 288 (quiet 69, cex 219)
- bug-class findings: 12
- class C: 7
- class D: 5
- determinism guard: PASS (all 300 cases ran the sim leg twice with identical summaries)

## Class C signatures (frontend/lowering gaps, deduped)

- `sva2check: N of N $check cells unsupported (sim leg)` -- 1 case(s); first repro: `/Users/npe/src/sv-yosys/third_party/yosys/tests/sva_fuzz/failures/C-1-176` (seed 1 index 176)
- `sva2check: unsupported sequence operator 'and' in $auto$` -- 6 case(s); first repro: `/Users/npe/src/sv-yosys/third_party/yosys/tests/sva_fuzz/failures/C-1-35` (seed 1 index 35)

## Class D findings

- seed 1 index 30: sim leg exit -6, no ERROR line
  - repro: `/Users/npe/src/sv-yosys/third_party/yosys/tests/sva_fuzz/failures/D-1-30`
- seed 1 index 143: sim leg exit -6, no ERROR line
  - repro: `/Users/npe/src/sv-yosys/third_party/yosys/tests/sva_fuzz/failures/D-1-143`
- seed 1 index 161: sim leg exit -6, no ERROR line
  - repro: `/Users/npe/src/sv-yosys/third_party/yosys/tests/sva_fuzz/failures/D-1-161`
- seed 1 index 229: sim leg exit -6, no ERROR line
  - repro: `/Users/npe/src/sv-yosys/third_party/yosys/tests/sva_fuzz/failures/D-1-229`
- seed 1 index 231: sim leg exit -11, no ERROR line
  - repro: `/Users/npe/src/sv-yosys/third_party/yosys/tests/sva_fuzz/failures/D-1-231`

## Full per-case log

| index | class | detail | wall s |
|---|---|---|---|
| 0 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 1 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 2 | agree-cex | cex at step 1 reproduced in sim | 0.10 |
| 3 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 4 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 5 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 6 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 7 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 8 | agree-cex | cex at step 4 reproduced in sim | 0.13 |
| 9 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 10 | agree-cex | cex at step 0 reproduced in sim | 0.12 |
| 11 | agree-cex | cex at step 0 reproduced in sim | 0.12 |
| 12 | agree-quiet | both legs quiet to bound 12 | 0.12 |
| 13 | agree-cex | cex at step 4 reproduced in sim | 0.12 |
| 14 | agree-cex | cex at step 7 reproduced in sim | 0.12 |
| 15 | agree-cex | cex at step 1 reproduced in sim | 0.13 |
| 16 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 17 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 18 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 19 | agree-quiet | both legs quiet to bound 12 | 0.12 |
| 20 | agree-cex | cex at step 6 reproduced in sim | 0.10 |
| 21 | agree-cex | cex at step 8 reproduced in sim | 0.10 |
| 22 | agree-cex | cex at step 5 reproduced in sim | 0.10 |
| 23 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 24 | agree-cex | cex at step 7 reproduced in sim | 0.10 |
| 25 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 26 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 27 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 28 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 29 | agree-cex | cex at step 6 reproduced in sim | 0.10 |
| 30 | D | sim leg exit -6, no ERROR line | 0.01 |
| 31 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 32 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 33 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 34 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 35 | C | sim-leg error sig: sva2check: unsupported sequence operator 'and' in $auto$ | 0.01 |
| 36 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 37 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 38 | agree-cex | cex at step 6 reproduced in sim | 0.10 |
| 39 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 40 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 41 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 42 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 43 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 44 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 45 | agree-cex | cex at step 2 reproduced in sim | 0.10 |
| 46 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 47 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 48 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 49 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 50 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 51 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 52 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 53 | agree-cex | cex at step 6 reproduced in sim | 0.10 |
| 54 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 55 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 56 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 57 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 58 | agree-cex | cex at step 4 reproduced in sim | 0.12 |
| 59 | agree-cex | cex at step 6 reproduced in sim | 0.10 |
| 60 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 61 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 62 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 63 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 64 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 65 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 66 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 67 | agree-cex | cex at step 10 reproduced in sim | 0.10 |
| 68 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 69 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 70 | agree-cex | cex at step 7 reproduced in sim | 0.10 |
| 71 | agree-cex | cex at step 11 reproduced in sim | 0.09 |
| 72 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 73 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 74 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 75 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 76 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 77 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 78 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 79 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 80 | agree-cex | cex at step 9 reproduced in sim | 0.09 |
| 81 | agree-quiet | both legs quiet to bound 12 | 0.14 |
| 82 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 83 | C | sim-leg error sig: sva2check: unsupported sequence operator 'and' in $auto$ | 0.01 |
| 84 | agree-cex | cex at step 6 reproduced in sim | 0.10 |
| 85 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 86 | agree-cex | cex at step 8 reproduced in sim | 0.10 |
| 87 | agree-cex | cex at step 2 reproduced in sim | 0.10 |
| 88 | agree-cex | cex at step 9 reproduced in sim | 0.11 |
| 89 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 90 | agree-cex | cex at step 9 reproduced in sim | 0.11 |
| 91 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 92 | agree-cex | cex at step 2 reproduced in sim | 0.10 |
| 93 | agree-cex | cex at step 4 reproduced in sim | 0.11 |
| 94 | agree-cex | cex at step 5 reproduced in sim | 0.10 |
| 95 | agree-cex | cex at step 7 reproduced in sim | 0.13 |
| 96 | agree-cex | cex at step 1 reproduced in sim | 0.11 |
| 97 | agree-cex | cex at step 6 reproduced in sim | 0.12 |
| 98 | agree-cex | cex at step 0 reproduced in sim | 0.11 |
| 99 | agree-cex | cex at step 6 reproduced in sim | 0.12 |
| 100 | agree-cex | cex at step 0 reproduced in sim | 0.11 |
| 101 | agree-cex | cex at step 8 reproduced in sim | 0.12 |
| 102 | agree-cex | cex at step 4 reproduced in sim | 0.12 |
| 103 | agree-cex | cex at step 4 reproduced in sim | 0.10 |
| 104 | agree-cex | cex at step 9 reproduced in sim | 0.10 |
| 105 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 106 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 107 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 108 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 109 | agree-cex | cex at step 10 reproduced in sim | 0.09 |
| 110 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 111 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 112 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 113 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 114 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 115 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 116 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 117 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 118 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 119 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 120 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 121 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 122 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 123 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 124 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 125 | agree-cex | cex at step 11 reproduced in sim | 0.10 |
| 126 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 127 | agree-cex | cex at step 8 reproduced in sim | 0.10 |
| 128 | agree-cex | cex at step 7 reproduced in sim | 0.08 |
| 129 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 130 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 131 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 132 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 133 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 134 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 135 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 136 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 137 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 138 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 139 | agree-cex | cex at step 8 reproduced in sim | 0.10 |
| 140 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 141 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 142 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 143 | D | sim leg exit -6, no ERROR line | 0.01 |
| 144 | agree-cex | cex at step 8 reproduced in sim | 0.10 |
| 145 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 146 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 147 | agree-cex | cex at step 9 reproduced in sim | 0.10 |
| 148 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 149 | agree-cex | cex at step 8 reproduced in sim | 0.10 |
| 150 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 151 | C | sim-leg error sig: sva2check: unsupported sequence operator 'and' in $auto$ | 0.01 |
| 152 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 153 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 154 | agree-cex | cex at step 6 reproduced in sim | 0.10 |
| 155 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 156 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 157 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 158 | agree-quiet | both legs quiet to bound 12 | 0.14 |
| 159 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 160 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 161 | D | sim leg exit -6, no ERROR line | 0.01 |
| 162 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 163 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 164 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 165 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 166 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 167 | agree-cex | cex at step 10 reproduced in sim | 0.18 |
| 168 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 169 | agree-cex | cex at step 9 reproduced in sim | 0.11 |
| 170 | agree-cex | cex at step 11 reproduced in sim | 0.10 |
| 171 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 172 | agree-cex | cex at step 7 reproduced in sim | 0.10 |
| 173 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 174 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 175 | agree-cex | cex at step 9 reproduced in sim | 0.09 |
| 176 | C | sim-leg unsupported sig: sva2check: N of N $check cells unsupported (sim leg) | 0.01 |
| 177 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 178 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 179 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 180 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 181 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 182 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 183 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 184 | agree-cex | cex at step 6 reproduced in sim | 0.10 |
| 185 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 186 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 187 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 188 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 189 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 190 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 191 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 192 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 193 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 194 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 195 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 196 | C | sim-leg error sig: sva2check: unsupported sequence operator 'and' in $auto$ | 0.01 |
| 197 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 198 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 199 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 200 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 201 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 202 | agree-quiet | both legs quiet to bound 12 | 0.13 |
| 203 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 204 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 205 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 206 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 207 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 208 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 209 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 210 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 211 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 212 | agree-cex | cex at step 8 reproduced in sim | 0.09 |
| 213 | agree-quiet | both legs quiet to bound 12 | 0.07 |
| 214 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 215 | agree-cex | cex at step 0 reproduced in sim | 0.10 |
| 216 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 217 | agree-cex | cex at step 8 reproduced in sim | 0.12 |
| 218 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 219 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 220 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 221 | agree-cex | cex at step 10 reproduced in sim | 0.11 |
| 222 | agree-quiet | both legs quiet to bound 12 | 0.12 |
| 223 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 224 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 225 | agree-cex | cex at step 7 reproduced in sim | 0.10 |
| 226 | agree-cex | cex at step 10 reproduced in sim | 0.10 |
| 227 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 228 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 229 | D | sim leg exit -6, no ERROR line | 0.01 |
| 230 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 231 | D | sim leg exit -11, no ERROR line | 0.01 |
| 232 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 233 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 234 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 235 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 236 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 237 | agree-cex | cex at step 11 reproduced in sim | 0.12 |
| 238 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 239 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 240 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 241 | agree-cex | cex at step 5 reproduced in sim | 0.11 |
| 242 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 243 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 244 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 245 | agree-cex | cex at step 3 reproduced in sim | 0.10 |
| 246 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 247 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 248 | C | sim-leg error sig: sva2check: unsupported sequence operator 'and' in $auto$ | 0.01 |
| 249 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 250 | agree-cex | cex at step 7 reproduced in sim | 0.10 |
| 251 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 252 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 253 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 254 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 255 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 256 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 257 | agree-cex | cex at step 6 reproduced in sim | 0.11 |
| 258 | C | sim-leg error sig: sva2check: unsupported sequence operator 'and' in $auto$ | 0.01 |
| 259 | agree-quiet | both legs quiet to bound 12 | 0.11 |
| 260 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 261 | agree-cex | cex at step 2 reproduced in sim | 0.09 |
| 262 | agree-cex | cex at step 5 reproduced in sim | 0.10 |
| 263 | agree-cex | cex at step 1 reproduced in sim | 0.10 |
| 264 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 265 | agree-cex | cex at step 2 reproduced in sim | 0.08 |
| 266 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 267 | agree-cex | cex at step 1 reproduced in sim | 0.08 |
| 268 | agree-cex | cex at step 6 reproduced in sim | 0.10 |
| 269 | agree-cex | cex at step 10 reproduced in sim | 0.12 |
| 270 | agree-cex | cex at step 10 reproduced in sim | 0.11 |
| 271 | agree-cex | cex at step 5 reproduced in sim | 0.10 |
| 272 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 273 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 274 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 275 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 276 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 277 | agree-quiet | both legs quiet to bound 12 | 0.09 |
| 278 | agree-cex | cex at step 10 reproduced in sim | 0.12 |
| 279 | agree-cex | cex at step 0 reproduced in sim | 0.09 |
| 280 | agree-cex | cex at step 3 reproduced in sim | 0.09 |
| 281 | agree-cex | cex at step 11 reproduced in sim | 0.11 |
| 282 | agree-cex | cex at step 9 reproduced in sim | 0.09 |
| 283 | agree-cex | cex at step 6 reproduced in sim | 0.09 |
| 284 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 285 | agree-cex | cex at step 7 reproduced in sim | 0.09 |
| 286 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
| 287 | agree-quiet | both legs quiet to bound 12 | 0.08 |
| 288 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 289 | agree-cex | cex at step 4 reproduced in sim | 0.09 |
| 290 | agree-cex | cex at step 5 reproduced in sim | 0.09 |
| 291 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 292 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 293 | agree-cex | cex at step 10 reproduced in sim | 0.09 |
| 294 | agree-cex | cex at step 1 reproduced in sim | 0.09 |
| 295 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 296 | agree-quiet | both legs quiet to bound 12 | 0.10 |
| 297 | agree-cex | cex at step 0 reproduced in sim | 0.08 |
| 298 | agree-cex | cex at step 3 reproduced in sim | 0.08 |
| 299 | agree-cex | cex at step 4 reproduced in sim | 0.08 |
