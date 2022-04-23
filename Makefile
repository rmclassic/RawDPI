SRC_FILES := $(filter-out $(wildcard *_tb.vhdl), $(wildcard *.vhdl))
SRC_TB_FILES := $(wildcard *_tb.vhdl)
OBJ_FILES := $(filter-out $(wildcard *.vhdl), $(wildcard *))
OBJ_FILES := $(filter-out Makefile, $(OBJ_FILES))

all : clean component tb

clean :
	rm -rf $(OBJ_FILES)

component :
	ghdl -a -fsynopsys $(SRC_FILES)

tb :
	$(foreach file,$(SRC_TB_FILES),ghdl -a -fsynopsys $(file);)
	$(foreach file,$(subst .vhdl,,$(SRC_TB_FILES)),ghdl -e -fsynopsys $(file);)
	ghdl -c -fsynopsys $(SRC_TB_FILES)
	$(foreach file,$(subst .vhdl,,$(SRC_TB_FILES)),ghdl -r $(file) --vcd=output_$(file).vcd;)
