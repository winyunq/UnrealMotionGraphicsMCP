// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"

class FFabWindowsClipboard
{
public:
    static bool GetBitmapFromClipboard(TArray<uint8>& OutPngBytes, int32& OutWidth, int32& OutHeight)
    {
#if PLATFORM_WINDOWS
        if (!OpenClipboard(GetActiveWindow())) return false;

        HBITMAP hBitmap = (HBITMAP)GetClipboardData(CF_BITMAP);
        if (hBitmap)
        {
            BITMAP bm;
            // Use ::GetObjectW (Unicode) explicitly as the GetObject macro is undefined by Unreal
            ::GetObjectW(hBitmap, sizeof(BITMAP), &bm);

            OutWidth = bm.bmWidth;
            OutHeight = bm.bmHeight;

            // Get Bits
            BITMAPINFO bmpInfo;
            ZeroMemory(&bmpInfo, sizeof(BITMAPINFO));
            bmpInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmpInfo.bmiHeader.biWidth = bm.bmWidth;
            bmpInfo.bmiHeader.biHeight = -bm.bmHeight; // Top-down
            bmpInfo.bmiHeader.biPlanes = 1;
            bmpInfo.bmiHeader.biBitCount = 32;
            bmpInfo.bmiHeader.biCompression = BI_RGB;

            TArray<uint8> Bits;
            Bits.AddUninitialized(bm.bmWidth * bm.bmHeight * 4);

            HDC hDC = GetDC(NULL);
            GetDIBits(hDC, hBitmap, 0, bm.bmHeight, Bits.GetData(), &bmpInfo, DIB_RGB_COLORS);
            ReleaseDC(NULL, hDC);

            // Convert BGRA (Windows) to RGBA (Unreal/PNG)
            for (int32 i = 0; i < Bits.Num(); i += 4)
            {
                uint8 B = Bits[i];
                uint8 R = Bits[i + 2];
                Bits[i] = R;
                Bits[i + 2] = B;
                Bits[i + 3] = 255; 
            }

            // Compress to PNG using ImageWrapper
            IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
            TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

            if (ImageWrapper.IsValid() && ImageWrapper->SetRaw(Bits.GetData(), Bits.Num(), bm.bmWidth, bm.bmHeight, ERGBFormat::RGBA, 8))
            {
                OutPngBytes = ImageWrapper->GetCompressed();
                CloseClipboard();
                return true;
            }
        }

        CloseClipboard();
#endif
        return false;
    }
};
