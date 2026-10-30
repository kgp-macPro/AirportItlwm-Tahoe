PRIVATE BE200 P7 — OFF/ON recovery correction; HARDWARE UNTESTED

P6 physically autojoined afterboot and afterone manualsleep/wake. OFFON was
sporadic. P7 fixes source-supported stale continuation and init scan-wait paths;
it is not yet a confirmed recovery fix. See BUILD_IDENTITY/P7_RECOVERY.md.

KGP only:back up entire currentP6 bundle+boot configuration. Replace complete
AirportItlwm.kext with this bundle at the SAME existingEFI location (no merge),
retain established restoredVentura legacy/rootpatch environment;reboot manually.
Verify loadedUUID,not Desktopfile. Rollback:restorewhole P6backup,reboot manually.
No tools here changeEFI,patches,networksettings,WiFi,powerstate orload a driver.

1. Save firstboot state before preconditioning. Record autojoin,IPv4/router and
   actualroute. Keep Ethernet traffic separate. No profiler or parallel scans.
2. Start boundary/log collector with VERIFIED current names (KGP en2/ARPT):
   sudo /usr/bin/env BE200_OUTPUT_PARENT="$HOME/Desktop/AirportItlwm/EVIDENCE/BE200_JOIN" WIFI_IFACE=en2 IOREG_WIFI_SERVICE=ARPT BE200_PASSIVE_CAPTURE=1 /bin/bash "$HOME/Desktop/AirportItlwm/BE200-EXPERIMENTAL/COLLECT_BE200_JOIN.sh"
   Other names are supported;explicitparams checked againstPCI/driver/primary
   interface. Never substitute Ethernet/AWDL or renameSSDT. Record priortraffic.
3. KGP manually toggles OFF,waits until interface is off,thenON once. Wait for
   scan/autojoin,record timing and pressReturn. Collector NEVER toggles orjoins.
   If it fails,leave that state unchanged and return original P7_JOIN ZIP first;
   do not hide it with repeated toggles. A succeeding latercycle is separate.
4. If firstcycle succeeds,KGP may repeat a few explicitly counted cycles,one
   capture perattempt includingbadcases. No broad42testcampaign requested.
5. Only after recovery evidence,one manualsleep/wake is a separate regression
   check. No claim based on UIalone:checkconfigdLinkActive,DHCPpacket/address/
   route. Internet/throughput remains a distinct optional test,not recoveryproof.

P6 remains archived. Publicalpha preparation is deferred untilKGPtestresult;
no publicready/ WiFi7/MLO/6GHz/BE201/AWDL/Continuity/BE200Bluetoothclaim.
