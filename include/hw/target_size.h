/*
 * target_size.h
 *
 * Copyright (C) 2017 AdaCore
 *
 * Developed by KONRAD Frederic <frederic.konrad@adacore.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/*
 * The main goal of this is to be able to detect the size of the data bus and be
 * maintainable (no rebase pain et all).
 */

#ifndef TARGET_SIZE_H
#define TARGET_SIZE_H

#define TARGET_IS_64BITS 0

#ifdef TARGET_AARCH64
#undef TARGET_IS_64BITS
#define TARGET_IS_64BITS 1
#endif /* TARGET_AARCH64 */

#ifdef TARGET_PPC64
#undef TARGET_IS_64BITS
#define TARGET_IS_64BITS 1
#endif /* TARGET_PPC64 */

#ifdef TARGET_X86_64
#undef TARGET_IS_64BITS
#define TARGET_IS_64BITS 1
#endif /* TARGET_X86_64 */

#endif
