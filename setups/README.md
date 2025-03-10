# Exemplary FPGA Board Setups

This folder provides exemplary NEORV32 SoC setups for different FPGA platforms/boards.
You can directly use one of the provided setups or use them as starting point to build your own setup.
Project maintainers may make pull requests against this repository to [add or link their setups](#Adding-Your-Project-Setup).


## Setups using Commercial Toolchains

| Setup | Toolchain | Board | FPGA  | Author(s) |
|:------|:----------|:------|:------|:----------|
| :file_folder: [`de0-nano-test-setup`](https://github.com/stnolting/neorv32/tree/master/setups/quartus/de0-nano-test-setup) | Intel Quartus Prime | [Terasic DE0-Nano](https://www.terasic.com.tw/cgi-bin/page/archive.pl?Language=English&CategoryNo=139&No=593)                     | Intel Cyclone IV `EP4CE22F17C6N`          | [stnolting](https://github.com/stnolting) |
| :file_folder: [`UPduino_v3`](https://github.com/stnolting/neorv32/tree/master/setups/radiant/UPduino_v3)                   | Lattice Radiant     | [tinyVision.ai Inc. UPduino `v3.0`](https://www.tindie.com/products/tinyvision_ai/upduino-v30-low-cost-lattice-ice40-fpga-board/) | Lattice iCE40 UltraPlus `iCE40UP5K-SG48I` | [stnolting](https://github.com/stnolting) |
| :file_folder: [`arty-a7-35-test-setup`](https://github.com/stnolting/neorv32/tree/master/setups/vivado/arty-a7-test-setup) | Xilinx Vivado       | [Digilent Arty A7-35](https://reference.digilentinc.com/reference/programmable-logic/arty-a7/start)                               | Xilinx Artix-7 `XC7A35TICSG324-1L`        | [stnolting](https://github.com/stnolting) |
| :file_folder: [`nexys-a7-test-setup`](https://github.com/stnolting/neorv32/tree/master/setups/vivado/nexys-a7-test-setup)  | Xilinx Vivado       | [Digilent Nexys A7](https://reference.digilentinc.com/reference/programmable-logic/nexys-a7/start)                                | Xilinx Artix-7 `XC7A50TCSG324-1`          | [AWenzel83](https://github.com/AWenzel83) |
| :file_folder: [`nexys-a7-test-setup`](https://github.com/stnolting/neorv32/tree/master/setups/vivado/nexys-a7-test-setup)  | Xilinx Vivado       | [Digilent Nexys 4 DDR](https://reference.digilentinc.com/reference/programmable-logic/nexys-4-ddr/start)                          | Xilinx Artix-7 `XC7A100TCSG324-1`         | [AWenzel83](https://github.com/AWenzel83) |


## Setups using Open-Source Toolchains (`osflow`)

| Setup | Toolchain | Board | FPGA  | Author(s) |
|:------|:----------|:------|:------|:----------|
| :file_folder: [`UPduino v3`](https://github.com/stnolting/neorv32/tree/master/setups/osflow)  | GHDL, Yosys, nextPNR | [UPduino v3.0](https://www.tindie.com/products/tinyvision_ai/upduino-v30-low-cost-lattice-ice40-fpga-board/) | Lattice iCE40 UltraPlus `iCE40UP5K-SG48I` | [tmeissner](https://github.com/tmeissner) |
| :file_folder: [`FOMU`](https://github.com/stnolting/neorv32/tree/master/setups/osflow)        | GHDL, Yosys, nextPNR | [FOMU](https://tomu.im/fomu.html)                                                                            | Lattice iCE40 UltraPlus `iCE40UP5K-SG48I` | [umarcor](https://github.com/umarcor) |
| :file_folder: [`iCESugar`](https://github.com/stnolting/neorv32/tree/master/setups/osflow)    | GHDL, Yosys, nextPNR | [iCESugar](https://github.com/wuxx/icesugar/blob/master/README_en.md)                                        | Lattice iCE40 UltraPlus `iCE40UP5K-SG48I` | [umarcor](https://github.com/umarcor) |
| :file_folder: [`Orange Crab`](https://github.com/stnolting/neorv32/tree/master/setups/osflow) | GHDL, Yosys, nextPNR | [Orange Crab](https://github.com/gregdavill/OrangeCrab)                                                      | Lattice ECP5-25F                          | [umarcor](https://github.com/umarcor), [jeremyherbert](https://github.com/jeremyherbert) |


### Adding Your Project Setup

Please respect the following guidelines if you'd like to add (or link) your setup to the list.

* check out the project's [code of conduct](https://github.com/stnolting/neorv32/tree/master/CODE_OF_CONDUCT.md)
* add a link if the board you are using provides online documentation (and/or can be purchased somewhere)
* use the :file_folder: emoji (`:file_folder:`) if the setup is located *in this* folder; use the :earth_africa: emoji (`:earth_africa:`) if it is a link to your local project
* please add a `README` to give some brief information about the setup and a `.gitignore` to keep things clean; take a look at [`arty-a7-35-test-setup`](https://github.com/stnolting/neorv32/setups/boards/arty-a7-35-test-setup) to get some ideas what a project setup might look like


### Setup-Specifc NEORV32 Software Framework Modification

In order to use the features provided by the setups, minor *optional* changes can be made to the default NEORV32 setup.

* To change the default data memory size take a look at the :books: User Guide section
[_General Software Framework Setup_](https://stnolting.github.io/neorv32/ug/#_general_software_framework_setup)
* To modify the SPI flash base address for storing/booting software application see :books: User Guide section
[_Customizing the Internal Bootloader_](https://stnolting.github.io/neorv32/ug/#_customizing_the_internal_bootloader)
