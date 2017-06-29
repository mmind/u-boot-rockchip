#!/bin/sh

../../03_rockchip/rkdeveloptool/rkdeveloptool db ../../03_rockchip/rkbin/rk33/rk3399_loader_v1.08.106.bin
sleep 2
../../03_rockchip/rkdeveloptool/rkdeveloptool wl 64 idbspl.img
../../03_rockchip/rkdeveloptool/rkdeveloptool wl 512 bl3.itb
../../03_rockchip/rkdeveloptool/rkdeveloptool rd

#./upgrade_tool DB ../../03_rockchip/rkbin/rk33/rk3399_loader_v1.08.106.bin
#sleep 2
#./upgrade_tool WL 64 idbspl.img
#./upgrade_tool WL 512 bl3.itb
#./upgrade_tool RD
