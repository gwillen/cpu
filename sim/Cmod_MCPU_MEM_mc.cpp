#include "Cmod_MCPU_MEM_mc.h"
#include "Sim.h"

Cmod_MCPU_MEM_mc::Cmod_MCPU_MEM_mc(Cmod_MCPU_MEM_mc_ports *_ports) :
	ports(_ports), 
	burst_cycrem(0),
	burst_rnw(0) {
	
	*ports->ltc2mc_avl_ready_0 = 0;
	*ports->ltc2mc_avl_rdata_valid_0 = 0;
	ports->ltc2mc_avl_rdata_0[0] = ports->ltc2mc_avl_rdata_0[1] = ports->ltc2mc_avl_rdata_0[2] = ports->ltc2mc_avl_rdata_0[3] = 0;
	
	ltc2mc_avl_read_req_0_last = ltc2mc_avl_write_req_0_last = 0;
}

/* WDatas are little endian. */
void Cmod_MCPU_MEM_mc::clk() {
	if (!*ports->ltc2mc_avl_ready_0) {
		/* Assert that nothing has changed.  Don't do any actual work if we're not ready! */
		SIM_CHECK((ltc2mc_avl_read_req_0_last == *ports->ltc2mc_avl_read_req_0) && "read request changed during not ready");
		SIM_CHECK((ltc2mc_avl_write_req_0_last == *ports->ltc2mc_avl_write_req_0) && "write request changed during not ready");
	} else {
		/* Check for burst validity */
		if (burst_cycrem) {
			if (burst_rnw) /* i.e., read */ {
				if (*ports->ltc2mc_avl_read_req_0)
					burst_cycrem--;
				SIM_CHECK(!*ports->ltc2mc_avl_write_req_0 && "write during read burst");
			} else /* i.e., write */ { 
				if (*ports->ltc2mc_avl_write_req_0)
					burst_cycrem--;
				SIM_CHECK(!*ports->ltc2mc_avl_read_req_0 && "read during write burst");
			}
			SIM_CHECK(!*ports->ltc2mc_avl_burstbegin_0 && "burst start during burst");
		} else if (*ports->ltc2mc_avl_burstbegin_0) {
			SIM_CHECK((*ports->ltc2mc_avl_read_req_0 ^ *ports->ltc2mc_avl_write_req_0) && "invalid burst start type");
			burst_cycrem = *ports->ltc2mc_avl_size_0 - 1;
			burst_rnw = *ports->ltc2mc_avl_read_req_0;
		} else
			SIM_CHECK(!*ports->ltc2mc_avl_read_req_0 && !*ports->ltc2mc_avl_write_req_0 && "read or write outside of burst");
	
		/* Dummy model: one-cycle memory */
		if (*ports->ltc2mc_avl_write_req_0) {
			uint32_t memad = *ports->ltc2mc_avl_addr_0 * 16;
			int i;
		
			for (i = 0; i < 16; i++)
				if (*ports->ltc2mc_avl_be_0 & (1 << i))
					memory[memad + i] = (ports->ltc2mc_avl_wdata_0[i / 4] >> ((i % 4) * 8)) & 0xFF;
		}
	}
	
	*ports->ltc2mc_avl_rdata_valid_0 = *ports->ltc2mc_avl_read_req_0 && *ports->ltc2mc_avl_ready_0;
	if (*ports->ltc2mc_avl_read_req_0) {
		uint32_t memad = *ports->ltc2mc_avl_addr_0 * 16;
		int i;
		
		for (i = 0; i < 4; i++)
			ports->ltc2mc_avl_rdata_0[i] =
				(memory[memad + i*4 + 0] <<  0) | (memory[memad + i*4 + 1] <<  8) |
				(memory[memad + i*4 + 2] << 16) | (memory[memad + i*4 + 3] << 24);
	}
	
	ltc2mc_avl_write_req_0_last = *ports->ltc2mc_avl_write_req_0;
	ltc2mc_avl_read_req_0_last = *ports->ltc2mc_avl_read_req_0;
	
	/* Set up ready for next time. */
	*ports->ltc2mc_avl_ready_0 = Sim::random(100) < 96;
}
