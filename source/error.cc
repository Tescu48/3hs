// vim: foldmethod=syntax
/* This file is part of 3hs
 * Copyright (C) 2021-2022 hShop developer team
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <3ds.h>

#include <unordered_map>

#include "error.hh"
#include "log.hh"

// Lookup maps

#define DEFAULT_RES "<Unknown>"

/* <module, <desc, msg>> */
static const std::unordered_map<Result, std::unordered_map<Result, const char *>> ERR_LOOKUP({
	{
		RM_KERNEL, {
			{ 2,    "Invalid DMA buffer memory permissions" },
			{ 1015, "Invalid handle"                        },
		}
	},
	{
		RM_OS, {
			{ 1 , "Out of synchronization object"             },
			{ 2 , "Out of shared memory objects"              },
			{ 9 , "Out of session objects"                    },
			{ 10, "Not enough memory for allocation"          },
			{ 20, "Wrong permissions for unprivileged access" },
			{ 26, "Session closed by remote process"          },
			{ 47, "Invalid command header"                    },
			{ 52, "Max port connections exceeded"             },
		}
	},
	{
		RM_FS, {
			{ 101, "Archive not mounted"                   },
			{ 120, "Doesn't exist / Failed to open"        },
			{ 141, "Game card not inserted"                },
			{ 171, "Bus: Busy / Underrun"                  },
			{ 172, "Bus: Illegal function"                 },
			{ 190, "Already exists / Failed to create"     },
			{ 210, "Partition full"                        },
			{ 230, "Illegal operation / File in use"       },
			{ 231, "Resource locked"                       },
			{ 250, "FAT operation denied"                  },
			{ 265, "Bus: Timeout"                          },
			{ 331, "Bus error / TWL partition invalid"     },
			{ 332, "Bus: Stop bit error"                   },
			{ 391, "Hash verification failure"             },
			{ 392, "RSA/Hash verification failure"         },
			{ 395, "Invalid RomFS or save data block hash" },
			{ 630, "Archive permission denied"             },
			{ 702, "Invalid path / Inaccessible archive"   },
			{ 705, "Offset out of bounds"                  },
			{ 721, "Reached file size limit"               },
			{ 760, "Unsupported operation"                 },
			{ 761, "ExeFS read size mismatch"              },

		}
	},
	{
		RM_SRV, {
			{ 5, "Invalid service name length" },
			{ 6, "Service access denied"       },
			{ 7, "String size mismatch"        },
		}
	},
	{
		RM_AM, {
			{ 2   , "Previous error invalidated context"  },
			{ 4   , "Wrong installation state"            },
			{ 8   , "non-linear write"                    },
			{ 18  , "finalization of incomplete install"  },
			{ 37  , "Invalid NCCH"                        },
			{ 39  , "Invalid or outdated title version"   },
			{ 41  , "Error type 1"                        },
			{ 43  , "Database does not exist"             },
			{ 44  , "Attempted to delete system title"    },
			{ 101 , "Error type -1"                       },
			{ 102 , "Error type -2"                       },
			{ 103 , "Error type -3"                       },
			{ 104 , "Error type -4"                       },
			{ 105 , "Error type -5"                       },
			{ 106 , "Bad signature/hash or devunit"       },
			{ 107 , "Error type -7"                       },
			{ 108 , "Error type -8"                       },
			{ 109 , "Error type -9"                       },
			{ 110 , "Error type -10"                      },
			{ 111 , "Error type -11"                      },
			{ 112 , "Error type -12"                      },
			{ 113 , "Error type -13"                      },
			{ 114 , "Error type -14"                      },
			{ 393 , "Invalid database"                    },
			{ 1020, "Already exists"                      },
		}
	},
	{
		RM_HTTP, {
			{ 3  , "POST data too large"              },
			{ 60 , "Failed to verify TLS certificate" },
			{ 70 , "Network unavailable"              },
			{ 72 , "Timed out"                        },
			{ 73 , "Failed to connect to host"        },
			{ 102, "Wrong context handle"             },
			{ 105, "Request timed out"                },
		}
	},
	{
		RM_MVD, {
			{ 271, "Invalid configuration" },
		}
	},
	{
		RM_NFC, {
			{ 512, "Invalid NFC state" },
		}
	},
	{
		RM_QTM, {
			{ 8, "Camera busy" },
		}
	},
	{
		RM_APPLICATION, {
			{ 0 , "Can't install n3ds exclusive games on an o3ds" },
			{ 1 , "Cancelled"                                     },
			{ 2 , "Too little free space on your SD Card"         },
			{ 3 , "Can't reinstall title unless asked"            },
			{ 4 , "Title count and list mismatch"                 },
			{ 5 , "Server doesn't support range"                  },
			{ 6 , "Server doesn't support length"                 },
			{ 7 , "Failed to parse server json"                   },
			{ 8 , "Server didn't return status code 200"          },
			{ 9 , "API failed to process request"                 },
			{ 10, "Log was too large to upload"                   },
			{ 11, "Failed to install file forwarder"              },
		}
	},
});

/* <lvl, msg> */
static const std::unordered_map<Result, const char *> LVL_LOOKUP({
	{ RL_SUCCESS     , "Success"      },
	{ RL_INFO        , "Info"         },
	{ RL_FATAL       , "Fatal"        },
	{ RL_RESET       , "Reset"        },
	{ RL_REINITIALIZE, "Reinitialize" },
	{ RL_USAGE       , "Usage"        },
	{ RL_PERMANENT   , "Permanent"    },
	{ RL_TEMPORARY   , "Temporary"    },
	{ RL_STATUS      , "Status"       },
});

/* <sum, msg> */
static const std::unordered_map<Result, const char *> SUM_LOOKUP({
	{ RS_SUCCESS      , "Success"          },
	{ RS_NOP          , "No operation"     },
	{ RS_WOULDBLOCK   , "Would block"      },
	{ RS_OUTOFRESOURCE, "Out of resource"  },
	{ RS_NOTFOUND     , "Not found"        },
	{ RS_INVALIDSTATE , "Invalid state"    },
	{ RS_NOTSUPPORTED , "Not supported"    },
	{ RS_INVALIDARG   , "Invalid argument" },
	{ RS_WRONGARG     , "Wrong argument"   },
	{ RS_CANCELED     , "Canceled"         },
	{ RS_STATUSCHANGED, "Status changed"   },
	{ RS_INTERNAL     , "Internal"         },
});

/* <module, msg> */
static const std::unordered_map<Result, const char *> MOD_LOOKUP({
	{ RM_COMMON       , "Common"        },
	{ RM_KERNEL       , "Kernel"        },
	{ RM_UTIL         , "Util"          },
	{ RM_FILE_SERVER  , "File server"   },
	{ RM_LOADER_SERVER, "Loader server" },
	{ RM_TCB          , "TCB"           },
	{ RM_OS           , "OS"            },
	{ RM_DBG          , "DBG"           },
	{ RM_DMNT         , "DMNT"          },
	{ RM_PDN          , "PDN"           },
	{ RM_GSP          , "GSP"           },
	{ RM_I2C          , "I2C"           },
	{ RM_GPIO         , "GPIO"          },
	{ RM_DD           , "DD"            },
	{ RM_CODEC        , "CODEC"         },
	{ RM_SPI          , "SPI"           },
	{ RM_PXI          , "PXI"           },
	{ RM_FS           , "FS"            },
	{ RM_DI           , "DI"            },
	{ RM_HID          , "HID"           },
	{ RM_CAM          , "CAM"           },
	{ RM_PI           , "PI"            },
	{ RM_PM           , "PM"            },
	{ RM_PM_LOW       , "PMLOW"         },
	{ RM_FSI          , "FSI"           },
	{ RM_SRV          , "SRV"           },
	{ RM_NDM          , "NDM"           },
	{ RM_NWM          , "NWM"           },
	{ RM_SOC          , "SOC"           },
	{ RM_LDR          , "LDR"           },
	{ RM_ACC          , "ACC"           },
	{ RM_ROMFS        , "RomFS"         },
	{ RM_AM           , "AM"            },
	{ RM_HIO          , "HIO"           },
	{ RM_UPDATER      , "Updater"       },
	{ RM_MIC          , "MIC"           },
	{ RM_FND          , "FND"           },
	{ RM_MP           , "MP"            },
	{ RM_MPWL         , "MPWL"          },
	{ RM_AC           , "AC"            },
	{ RM_HTTP         , "HTTP"          },
	{ RM_DSP          , "DSP"           },
	{ RM_SND          , "SND"           },
	{ RM_DLP          , "DLP"           },
	{ RM_HIO_LOW      , "RM_HIO_LOW"    },
	{ RM_CSND         , "CSND"          },
	{ RM_SSL          , "SSL"           },
	{ RM_AM_LOW       , "AMLOW"         },
	{ RM_NEX          , "NEX"           },
	{ RM_FRIENDS      , "Friends"       },
	{ RM_RDT          , "RDT"           },
	{ RM_APPLET       , "Applet"        },
	{ RM_NIM          , "NIM"           },
	{ RM_PTM          , "PTM"           },
	{ RM_MIDI         , "MIDI"          },
	{ RM_MC           , "MC"            },
	{ RM_SWC          , "SWC"           },
	{ RM_FATFS        , "FatFS"         },
	{ RM_NGC          , "NGC"           },
	{ RM_CARD         , "CARD"          },
	{ RM_CARDNOR      , "CARDNOR"       },
	{ RM_SDMC         , "SDMC"          },
	{ RM_BOSS         , "BOSS"          },
	{ RM_DBM          , "DBM"           },
	{ RM_CONFIG       , "Config"        },
	{ RM_PS           , "PS"            },
	{ RM_CEC          , "CEC"           },
	{ RM_IR           , "IR"            },
	{ RM_UDS          , "UDS"           },
	{ RM_PL           , "PL"            },
	{ RM_CUP          , "CUP"           },
	{ RM_GYROSCOPE    , "Gyroscope"     },
	{ RM_MCU          , "MCU"           },
	{ RM_NS           , "NS"            },
	{ RM_NEWS         , "NEWS"          },
	{ RM_RO           , "RO"            },
	{ RM_GD           , "GD"            },
	{ RM_CARD_SPI     , "CARDSPI"       },
	{ RM_EC           , "EC"            },
	{ RM_WEB_BROWSER  , "Web browser"   },
	{ RM_TEST         , "TEST"          },
	{ RM_ENC          , "ENC"           },
	{ RM_PIA          , "PIA"           },
	{ RM_ACT          , "ACT"           },
	{ RM_VCTL         , "VCTL"          },
	{ RM_OLV          , "OLV"           },
	{ RM_NEIA         , "NEIA"          },
	{ RM_NPNS         , "NPNS"          },
	{ RM_AVD          , "AVD"           },
	{ RM_L2B          , "L2B"           },
	{ RM_MVD          , "MVD"           },
	{ RM_NFC          , "NFC"           },
	{ RM_UART         , "UART"          },
	{ RM_SPM          , "SPM"           },
	{ RM_QTM          , "QTM"           },
	{ RM_NFP          , "NFP"           },
	{ RM_APPLICATION  , "Application"   },
});

// Getters


static void get_lvl(Result& res, error_container& ret)
{
	ret.iLvl = R_LEVEL(res);
	auto it = LVL_LOOKUP.find(ret.iLvl);
	if(it == LVL_LOOKUP.end()) ret.sLvl = DEFAULT_RES;
	else ret.sLvl = it->second;
}

static void get_sum(Result& res, error_container& ret)
{
	ret.iSum = R_SUMMARY(res);
	auto it = SUM_LOOKUP.find(ret.iSum);
	if(it == SUM_LOOKUP.end()) ret.sSum = DEFAULT_RES;
	else ret.sSum = it->second;
}

static void get_mod(Result& res, error_container& ret)
{
	ret.iMod = R_MODULE(res);
	auto it = MOD_LOOKUP.find(ret.iMod);
	if(it == MOD_LOOKUP.end()) ret.sMod = DEFAULT_RES;
	else ret.sMod = it->second;
}

static void get_desc(Result& res, error_container& ret)
{
	ret.iDesc = R_DESCRIPTION(res);
	auto mIt = ERR_LOOKUP.find(R_MODULE(res));
	if(mIt == ERR_LOOKUP.end()) ret.sDesc = DEFAULT_RES;
	else
	{
		auto dIt = mIt->second.find(ret.iDesc);
		if(dIt == mIt->second.end()) ret.sDesc = DEFAULT_RES;
		else ret.sDesc = dIt->second;
	}
}


error_container get_error(Result res)
{
	error_container ret;

	get_desc(res, ret);
	get_lvl(res, ret);
	get_sum(res, ret);
	get_mod(res, ret);
	ret.full = res;

	return ret;
}

std::string pad8code(Result code)
{
	char errpad[9]; snprintf(errpad, 9, "%08lX", (unsigned long) code);
	return errpad;
}

std::string format_err(const std::string& msg, Result code)
{
	return msg + " (" + std::to_string(code) + ")";
}

void report_error(error_container& container, std::string note)
{
	elog("===========================");
	elog("| ERROR REPORT            |");
	elog("===========================");
if(note != "") {
	elog("Note        : %s",            note.c_str()); }
	elog("Result Code : 0x%08lX",       container.full);
	elog("Description : %s (0x%08lX)",  container.sDesc.c_str(), container.iDesc);
	elog("Module      : %s (0x%08lX)",  container.sMod.c_str(), container.iMod);
	elog("Level       : %s (0x%08lX)",  container.sLvl.c_str(), container.iLvl);
	elog("Summary     : %s (0x%08lX)",  container.sSum.c_str(), container.iSum);
	elog("===========================");
	log_flush();
}

