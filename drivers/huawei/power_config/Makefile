obj-$(CONFIG_HW_POWER_TREE) += hw_power.o

GET_LOCAL_DIR = $(patsubst %/, %, $(dir $(word $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST))))

power_hsad:
	$(shell cd $(GET_LOCAL_DIR);perl parse_power_tree_new.pl ../../../../device/huawei/customize/hsad > /dev/null 2>&1)
