param([string]$Profile = "11. VNPT Wifi 8 5GHz")
# Connect the first WLAN interface to a saved profile through wlanapi.dll WlanConnect
# (profile mode: no scan, no BSS list), for when netsh is blocked by the Location setting.
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public static class Wlan {
    [DllImport("wlanapi.dll")] public static extern int WlanOpenHandle(uint v, IntPtr r, out uint nv, out IntPtr h);
    [DllImport("wlanapi.dll")] public static extern int WlanCloseHandle(IntPtr h, IntPtr r);
    [DllImport("wlanapi.dll")] public static extern int WlanEnumInterfaces(IntPtr h, IntPtr r, out IntPtr list);
    [DllImport("wlanapi.dll")] public static extern void WlanFreeMemory(IntPtr p);
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct ConnParams {
        public int mode; public string profile; public IntPtr ssid; public IntPtr bssList;
        public int bssType; public uint flags;
    }
    [DllImport("wlanapi.dll")] public static extern int WlanConnect(IntPtr h, ref Guid g, ref ConnParams p, IntPtr r);
}
"@
$nv = 0; $h = [IntPtr]::Zero
$rc = [Wlan]::WlanOpenHandle(2, [IntPtr]::Zero, [ref]$nv, [ref]$h)
if ($rc -ne 0) { "WlanOpenHandle rc=$rc"; exit 1 }
$list = [IntPtr]::Zero
$rc = [Wlan]::WlanEnumInterfaces($h, [IntPtr]::Zero, [ref]$list)
if ($rc -ne 0) { "WlanEnumInterfaces rc=$rc"; exit 1 }
$n = [Runtime.InteropServices.Marshal]::ReadInt32($list)
if ($n -lt 1) { "no WLAN interface"; exit 1 }
# WLAN_INTERFACE_INFO_LIST: DWORD count, DWORD index, then WLAN_INTERFACE_INFO[] (GUID first)
$gbytes = New-Object byte[] 16
[Runtime.InteropServices.Marshal]::Copy([IntPtr]($list.ToInt64() + 8), $gbytes, 0, 16)
$guid = New-Object Guid (,$gbytes)
$state = [Runtime.InteropServices.Marshal]::ReadInt32([IntPtr]($list.ToInt64() + 8 + 16 + 512))
[Wlan]::WlanFreeMemory($list)
$p = New-Object Wlan+ConnParams
$p.mode = 0          # wlan_connection_mode_profile
$p.profile = $Profile
$p.bssType = 1       # dot11_BSS_type_infrastructure
$rc = [Wlan]::WlanConnect($h, [ref]$guid, [ref]$p, [IntPtr]::Zero)
[void][Wlan]::WlanCloseHandle($h, [IntPtr]::Zero)
"interface $guid state_before=$state WlanConnect rc=$rc"
