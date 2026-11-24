/*
 * SPDX-FileCopyrightText: 2026 VIEWE
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#if !defined(BSP_CONFIG_NO_GRAPHIC_LIB)
#ifdef CONFIG_BSP_NO_GRAPHIC_LIB
#define BSP_CONFIG_NO_GRAPHIC_LIB (1)
#else
#define BSP_CONFIG_NO_GRAPHIC_LIB (0)
#endif
#endif
