#include "stdafx.h"
#include "TestManager.h"
#include "BT_Test.h"
#include "BT_Test_Internal.h"
#include "IQmeasure.h"
#include "vDUT.h"
#include "math.h"

using namespace std;

// These global variables are declared in BT_Test_Internal.cpp
extern TM_ID                 g_BT_Test_ID;
extern vDUT_ID               g_BT_Dut;

// This global variable is declared in BT_Global_Setting.cpp
extern BT_GLOBAL_SETTING g_BTGlobalSettingParam;

#pragma region Define Input and Return structures (two containers and two structs)
// Input Parameter Container
map<string, BT_SETTING_STRUCT> l_txVerifyLEParamMap;

// Return Value Container
map<string, BT_SETTING_STRUCT> l_txVerifyLEReturnMap;

struct tagParam
{
	// Mandatory Parameters
	int       ANALYZE_POWER_ONLY;
	int       TRANSMIT_0XF0_SEQUENCE_FOR_DELTA_F1_AVG;
	int       FREQ_MHZ;								/*! The center frequency (MHz). */
	char      PACKET_TYPE[MAX_BUFFER_SIZE];			/*! The pack type to verify LE. */
	int		  PACKET_LENGTH;						/*! The number of octets in one packet to verify LE. */
	int       TX_POWER_LEVEL;						/*! The output power to verify LE. */
	double    EXPECTED_TX_POWER_DBM;                /*! The expected TX power dBm at TX_POWER_LEVEL. */
	double    CABLE_LOSS_DB;						/*! The path loss of test system. */
	double    SAMPLING_TIME_US;						/*! The sampling time to verify LE. */

} l_txVerifyLEParam;

struct tagReturn
{
	// <Perform LP_AnalyzePower>
	double   POWER_AVERAGE_DBM;
	double   POWER_PEAK_DBM;
	double   TARGET_POWER_DBM;
	// <Perform LP_AnalyzeBluetooth>
	double   DATA_RATE_DETECT;

	double   DELTA_F1_AVERAGE;
	double   DELTA_F2_MAX;
	double   DELTA_F2_AVERAGE;
	double   Fn_MAX;
	double   DELTA_F0_Fn_MAX;
	double   DELTA_F1_F0;
	double   DELTA_Fn_Fn5_MAX;
	double   FREQ_DEV_SYNC_AVG;
	double   DELTA_F2_F1_AV_RATIO;

	int      CRC_OK;
	double   FREQ_OFFSET;
	double	 MAX_POWER_ACP_DBM[BT_ACP_SECTION];

	double   CABLE_LOSS_DB;						/*! The path loss of test system. */
	char     ERROR_MESSAGE[MAX_BUFFER_SIZE];
} l_txVerifyLEReturn;
#pragma endregion

#ifndef WIN32
int initTXVerifyLEContainers = InitializeTXVerifyLEContainers();
#endif

int ClearTxVerifyLEReturn(void)
{
	l_txVerifyLEParamMap.clear();
	l_txVerifyLEReturnMap.clear();
	return 0;
}

//! BT TX Verify LE
/*!
 * Input Parameters
 *
 *  - Mandatory
 *      -# FREQ_MHZ (double): The center frequency (MHz)
 *      -# TX_POWER (double): The power (dBm) DUT is going to transmit at the antenna port
 *
 * Return Values
 *      -# A string for error message
 *
 * \return 0 No error occurred
 * \return -1 Error(s) occurred.  Please see the returned error message for details
 */


BT_TEST_API int BT_TX_Verify_LE(void)
{
	int    err = ERR_OK;

	bool   analysisOK = false, captureOK  = false, vDutActived = false;
	int    dummyValue   = 0;
	int	   packetLength = 0;
	int    avgIteration = 0;
	double dummyMax     = 0;
	double dummyMin     = 0;
	double samplingTimeUs = 0, cableLossDb = 0;
	char   vDutErrorMsg[MAX_BUFFER_SIZE] = {'\0'};
	char   sigFileNameBuffer[MAX_BUFFER_SIZE] = {'\0'};
	char   logMessage[MAX_BUFFER_SIZE] = {'\0'};

	/*---------------------------------------*
	 * Clear Return Parameters and Container *
	 *---------------------------------------*/
	ClearReturnParameters(l_txVerifyLEReturnMap);

	/*------------------------*
	 * Respond to QUERY_INPUT *
	 *------------------------*/
	err = TM_GetIntegerParameter(g_BT_Test_ID, "QUERY_INPUT", &dummyValue);
	if( ERR_OK==err )
	{
		RespondToQueryInput(l_txVerifyLEParamMap);
		return err;
	}
	else
	{
		// do nothing
	}

	/*-------------------------*
	 * Respond to QUERY_RETURN *
	 *-------------------------*/
	err = TM_GetIntegerParameter(g_BT_Test_ID, "QUERY_RETURN", &dummyValue);
	if( ERR_OK==err )
	{
		RespondToQueryReturn(l_txVerifyLEReturnMap);
		return err;
	}
	else
	{
		// do nothing
	}

	/*----------------------------------------------------------------------------------------
	 * Declare vectors for storing test results: vector< double >
	 *-----------------------------------------------------------------------------------------*/
	vector< double >		powerAvEachBurst	(g_BTGlobalSettingParam.TX_LE_AVERAGE);
	vector< double >		powerPkEachBurst	(g_BTGlobalSettingParam.TX_LE_AVERAGE);
	vector< double >		leDeltaF1Avg	    (g_BTGlobalSettingParam.TX_LE_AVERAGE);
	vector< double >		LeFreqOffset		(g_BTGlobalSettingParam.TX_LE_AVERAGE);
	vector< double >		leDeltaF2Max		(g_BTGlobalSettingParam.TX_LE_AVERAGE);
	vector< double >		leDeltaF2Avg		(g_BTGlobalSettingParam.TX_LE_AVERAGE);
	vector< double >		leFnMax	            (g_BTGlobalSettingParam.TX_LE_AVERAGE);
	vector< double >		leDeltaF0FnMax		(g_BTGlobalSettingParam.TX_LE_AVERAGE);
	vector< double >		leDeltaF1F0		    (g_BTGlobalSettingParam.TX_LE_AVERAGE);
	vector< double >		leDeltaFnFn_5Max    (g_BTGlobalSettingParam.TX_LE_AVERAGE);
	vector< double >		leFreqDevSyncAvg    (g_BTGlobalSettingParam.TX_LE_AVERAGE);
	vector< vector<double> >    maxPowerAcpDbm	(BT_ACP_SECTION, vector<double>(g_BTGlobalSettingParam.TX_LE_AVERAGE));

	//	l_txVerifyLEReturn.PAYLOAD_ERRORS = 0;
	l_txVerifyLEReturn.CRC_OK = 1;

	try
	{
		//IQapi can handle this now.
		///*-----------------------------------------------------------*
		//      * Only IQ201X support BT LE, Zhiyong Huang on Jan 26, 2011 *
		//      *-----------------------------------------------------------*/

		//      if (g_BTGlobalSettingParam.IQ_TESTER_TYPE != TESTER_TYPE_2010) //Only IQ201X can support dirty packet in MPS mode
		//      {
		//          err = -1;
		//          LogReturnMessage(logMessage, MAX_BUFFER_SIZE, LOGGER_ERROR, "[BT_LE_DP] Tester is not IQ201X, it cannot support BT LE test.\n");
		//          throw logMessage;
		//      }
		//      else
		//      {
		//          //do nothing
		//      }

		/*-----------------------------------------------------------*
		 * Both g_BT_Test_ID and g_BT_Dut need to be valid (>=0) *
		 *-----------------------------------------------------------*/
		TM_ClearReturns(g_BT_Test_ID);
		if( g_BT_Test_ID<0 || g_BT_Dut<0 )
		{
			err = -1;
			LogReturnMessage(logMessage, MAX_BUFFER_SIZE, LOGGER_ERROR, "[BT] BT_Test_ID or BT_Dut not valid.\n");
			throw logMessage;
		}
		else
		{
			// do nothing
		}

		/*----------------------*
		 * Get input parameters *
		 *----------------------*/
		err = GetInputParameters(l_txVerifyLEParamMap);
		if ( ERR_OK!=err )
		{
			LogReturnMessage(logMessage, MAX_BUFFER_SIZE, LOGGER_ERROR, "[BT] Input parameters are not completed.\n");
			throw logMessage;
		}

		// Check path loss (by ant and freq)
		if ( 0==l_txVerifyLEParam.CABLE_LOSS_DB )
		{
			err = TM_GetPathLossAtFrequency(g_BT_Test_ID, l_txVerifyLEParam.FREQ_MHZ, &l_txVerifyLEParam.CABLE_LOSS_DB, 0, TX_TABLE);
			if ( ERR_OK!=err )
			{
				LogReturnMessage(logMessage, MAX_BUFFER_SIZE, LOGGER_ERROR, "[BT] Failed to get CABLE_LOSS_DB from path loss table.\n", err);
				throw logMessage;
			}
		}
		else
		{
			// do nothing
		}

		/*----------------------------*
		 * Disable VSG output signal  *
		 *----------------------------*/
		// make sure no signal is generated by the VSG
		err = ::LP_EnableVsgRF(0);
		if ( ERR_OK!=err )
		{
			LogReturnMessage(logMessage, MAX_BUFFER_SIZE, LOGGER_ERROR, "[BT] Fail to turn off VSG, LP_EnableVsgRF(0) return error.\n");
			throw logMessage;
		}
		else
		{
			LogReturnMessage(logMessage, MAX_BUFFER_SIZE, LOGGER_INFORMATION, "[BT] Turn off VSG LP_EnableVsgRF(0) return OK.\n");
		}

#pragma region Configure DUT to transmit
		/*-------------------------------------------*
		 * Configure DUT to transmit - PRBS9 Pattern *
		 *-------------------------------------------*/
		// Set DUT RF frequency, Tx power, data rate
		// And clear vDUT parameters at beginning.
		vDUT_ClearParameters(g_BT_Dut);

		//check whether it is a supported frequency/channel.
		if (l_txVerifyLEParam.FREQ_MHZ>= 2402 && l_txVerifyLEParam.FREQ_MHZ <= 2480)
		{
			if( 0 == l_txVerifyLEParam.FREQ_MHZ%2)
			{
				vDUT_AddIntegerParameter(g_BT_Dut, "FREQ_MHZ",			l_txVerifyLEParam.FREQ_MHZ);	//right channel
			}
			else
			{
				LogReturnMessage(logMessage, MAX_BUFFER_SIZE, LOGGER_ERROR, "[BT] %d MHz is not supported in BT LE, channel step is 2MHz\n", l_txVerifyLEParam.FREQ_MHZ);
				throw logMessage; //do nothing
			}
		}
		else
		{
			LogReturnMessage(logMessage, MAX_BUFFER_SIZE, LOGGER_ERROR, "[BT] %d MHz is not supported in BT LE, out of range! \n", l_txVerifyLEParam.FREQ_MHZ);
			throw logMessage;
		}


		vDUT_AddIntegerParameter(g_BT_Dut, "MODULATION_TYPE",   BT_LE_PATTERN_PRBS9);	//{0x00, "PRBS9 Pattern"}

		vDUT_AddIntegerParameter(g_BT_Dut, "LOGIC_CHANNEL",     1);	//{0x01, "ACL Basic"} //not needed for LE

		vDUT_AddStringParameter (g_BT_Dut, "PACKET_TYPE",	    l_txVerifyLEParam.PACKET_TYPE);

		// Check packet length
		if (0==l_txVerifyLEParam.PACKET_LENGTH)
		{
			GetPacketLength("PER", "PACKETS_LENGTH", l_txVerifyLEParam.PACKET_TYPE, &packetLength);
		}
		else
		{
			packetLength = l_txVerifyLEParam.PACKET_LENGTH;
		}
		vDUT_AddIntegerParameter(g_BT_Dut, "PACKET_LENGTH",     packetLength);

		//not needed for LE test
		vDUT_AddIntegerParameter(g_BT_Dut, "TX_POWER_LEVEL",	l_txVerifyLEParam.TX_POWER_LEVEL);
		vDUT_AddDoubleParameter(g_BT_Dut, "EXPECTED_TX_POWER_DBM", l_txVerifyLEParam.EXPECTED_TX_POWER_DBM);

		//LE does not have power control now, set target power same as  EXPECTED_TX_POWER_DBM
		l_txVerifyLEReturn.TARGET_POWER_DBM = l_txVerifyLEParam.EXPECTED_TX_POWER_DBM;

		err = vDUT_Run(g_BT_Dut, "QUERY_POWER_DBM");
		if ( ERR_OK!=err )
		{	// Check if vDUT returns "ERROR_MESSAGE" or not; if "Yes", then the error message must be handled and returned to the upper layer.
			err = vDUT_GetStringReturn(g_BT_Dut, "ERROR_MESSAGE", vDutErrorMsg, MAX_BUFFER_SIZE);
			if ( ERR_OK==err )	// Get "ERROR_MESSAGE" from vDUT
			{
				err = -1;	// set err to -1 indicates "Error".
				LogReturnMessage(logMessage, MAX_BUFFER_SIZE, LOGGER_ERROR, vDutErrorMsg);
				throw logMessage;
			}
			else	// Return generic error message to the upper layer.
			{
				LogReturnMessage(logMessage, MAX_BUFFER_SIZE, LOGGER_ERROR, "[BT] vDUT_Run(QUERY_POWER_DBM) return error.\n");
				throw logMessage;
			}
		}
		else
		{
			// do nothing
		}

		err = vDUT_GetDoubleReturn(g_BT_Dut, "POWER_DBM", &l_txVerifyLEReturn.TARGET_POWER_DBM);
		if ( ERR_OK!=err )
		{
			LogReturnMessage(logMessage, MAX_BUFFER_SIZE, LOGGER_ERROR, "[BT] vDUT_GetDoubleReturn(POWER_DBM) return error.\n");
			throw logMessage;
		}

		err = vDUT_Run(g_BT_Dut, "TX_START");
		if ( ERR_OK!=err )
		{	// Check if vDUT returns "ERROR_MESSAGE" or not; if "Yes", then the error message must be handled and returned to the upper layer.
			vDutActived = false;
			err = vDUT_GetStringReturn(g_BT_Dut, "ERROR_MESSAGE", vDutErrorMsg, MAX_BUFFER_SIZE);
			if ( ERR_OK==err )	// Get "ERROR_MESSAGE" from vDUT
			{
				err = -1;	// set err to -1 indicates "Error"
				LogReturnMessage(logMessage, MAX_BUFFER_SIZE, LOGGER_ERROR, vDutErrorMsg);
				throw logMessage;
			}
			else	// Return generic error message to the upper layer
			{
				LogReturnMessage(logMessage, MAX_BUFFER_SIZE, LOGGER_ERROR, "[BT] vDUT_Run(TX_START) return error.\n");
				throw logMessage;
			}
		}
		else
		{
			vDutActived = true;
		}

		// Delay for DUT settle
		if (0!=g_BTGlobalSettingParam.DUT_TX_SETTLE_TIME_MS)
		{
			Sleep(g_BTGlobalSettingParam.DUT_TX_SETTLE_TIME_MS);
		}
		else
		{
			// do nothing
		}
#pragma endregion

#pragma region Setup LP Tester and Capture
		/*--------------------*
		 * Setup IQtester VSA *
		 *--------------------*/
		double peakToAvgRatio = g_BTGlobalSettingParam.IQ_P_TO_A_LE;
		cableLossDb = l_txVerifyLEParam.CABLE_LOSS_DB;

		err = LP_SetVsaBluetooth(  l_txVerifyLEParam.FREQ_MHZ*1e6,
				l_txVerifyLEReturn.TARGET_POWER_DBM-cableLossDb+peakToAvgRatio,
				g_BTGlobalSettingParam.VSA_PORT,
				g_BTGlobalSettingParam.VSA_TRIGGER_LEVEL_DB,
				g_BTGlobalSettingParam.VSA_PRE_TRIGGER_TIME_US/1000000
				);
		if ( ERR_OK!=err )
		{
			LogReturnMessage(logMessage, MAX_BUFFER_SIZE, LOGGER_ERROR, "[BT] Fail to setup VSA.\n");
			throw logMessage;
		}

		// Check capture time
		if (0==l_txVerifyLEParam.SAMPLING_TIME_US)
		{
			samplingTimeUs = PacketTypeToSamplingTimeUs(l_txVerifyLEParam.PACKET_TYPE);
		}
		else	// SAMPLING_TIME_US != 0
		{
			samplingTimeUs = l_txVerifyLEParam.SAMPLING_TIME_US;
		}

		if ( 1==l_txVerifyLEParam.ANALYZE_POWER_ONLY )
		{
			/*--------------------------------*
			 * Start "while" loop for average *
			 *--------------------------------*/
			avgIteration = 0;
			while ( avgIteration<g_BTGlobalSettingParam.TX_LE_AVERAGE )
			{
				analysisOK = false;
				captureOK  = false;

				/*----------------------------*
				 * Perform normal VSA capture *
				 *----------------------------*/
				err = LP_VsaDataCapture( samplingTimeUs/1000000, g_BTGlobalSettingParam.VSA_TRIGGER_TYPE );
				if( ERR_OK!=err )	// capture is failed
				{
					// Fail Capture
					LogReturnMessage(logMessage, MAX_BUFFER_SIZE, LOGGER_ERROR, "[BT] Fail to capture signal at %d MHz.\n", l_txVerifyLEParam.FREQ_MHZ);
					throw logMessage;
				}
				else
				{
					LogReturnMessage(logMessage, MAX_BUFFER_SIZE, LOGGER_INFORMATION, "[BT] LP_VsaDataCapture() at %d MHz return OK.\n", l_txVerifyLEParam.FREQ_MHZ);
				}

				/*--------------*
				 *  Capture OK  *
				 *--------------*/
				captureOK = true;
				if (1==g_BTGlobalSettingParam.VSA_SAVE_CAPTURE_ALWAYS)
				{
					// TODO: must give a warning that VSA_SAVE_CAPTURE_ALWAYS is ON
					sprintf_s(sigFileNameBuffer, MAX_BUFFER_SIZE, "%s_%d_%s", "BT_TX_LE_PWR_SaveAlways", l_txVerifyLEParam.FREQ_MHZ, l_txVerifyLEParam.PACKET_TYPE);
					BTSaveSigFile(sigFileNameBuffer);
				}
				else
				{
					// do nothing
				}

				/*--------------------------*
				 *  Perform power analysis  *
				 *--------------------------*/
				double dummy_T_INTERVAL      = 3.2;
				double dummy_MAX_POW_DIFF_DB = 15.0;
				err = LP_AnalyzePower( dummy_T_INTERVAL/1000000, dummy_MAX_POW_DIFF_DB );
				if (ERR_OK!=err)
				{	// Fail Analysis, thus save capture (Signal File) for debug
					sprintf_s(sigFileNameBuffer, MAX_BUFFER_SIZE, "%s_%d_%s", "BT_TX_POWER_AnalysisFailed", l_txVerifyLEParam.FREQ_MHZ, l_txVerifyLEParam.PACKET_TYPE);
					BTSaveSigFile(sigFileNameBuffer);
					LogReturnMessage(logMessage, MAX_BUFFER_SIZE, LOGGER_ERROR, "[BT] LP_AnalyzePower() return error.\n");
					throw logMessage;
				}
				else
				{
					// do nothing
				}

#pragma region Retrieve analysis Results
				/*-----------------------------*
				 *  Retrieve analysis Results  *
				 *-----------------------------*/
				analysisOK = true;

				// powerAvEachBurst
				powerAvEachBurst[avgIteration] = LP_GetScalarMeasurement("P_av_no_gap_all_dBm",0);
				if ( -99.00 >= powerAvEachBurst[avgIteration] )
				{
					analysisOK = false;
					l_txVerifyLEReturn.POWER_AVERAGE_DBM = NA_NUMBER;
					sprintf_s(sigFileNameBuffer, MAX_BUFFER_SIZE, "%s_%d_%s", "BT_TX_POWER_AnalysisFailed", l_txVerifyLEParam.FREQ_MHZ, l_txVerifyLEParam.PACKET_TYPE);
					BTSaveSigFile(sigFileNameBuffer);
					err = -1;
					LogReturnMessage(logMessage, MAX_BUFFER_SIZE, LOGGER_ERROR, "LP_GetScalarMeasurement(P_av_no_gap_all_dBm) return error.\n");
					throw logMessage;
				}
				else
				{
					powerAvEachBurst[avgIteration] = powerAvEachBurst[avgIteration] + cableLossDb;
				}

				// powerPkEachBurst
				powerPkEachBurst[avgIteration] = LP_GetScalarMeasurement("P_pk_each_burst_dBm",0);
				if ( -99.00 >= powerPkEachBurst[avgIteration] )
				{
					analysisOK = false;
					l_txVerifyLEReturn.POWER_PEAK_DBM = NA_NUMBER;
					sprintf_s(sigFileNameBuffer, MAX_BUFFER_SIZE, "%s_%d_%s", "BT_TX_POWER_AnalysisFailed", l_txVerifyLEParam.FREQ_MHZ, l_txVerifyLEParam.PACKET_TYPE);
					BTSaveSigFile(sigFileNameBuffer);
					err = -1;
					LogReturnMessage(logMessage, MAX_BUFFER_SIZE, LOGGER_ERROR, "LP_GetScalarMeasurement(P_pk_each_burst_dBm) return error.\n");
					throw logMessage;
				}
				else
				{
					powerPkEachBurst[avgIteration] = powerPkEachBurst[avgIteration] + cableLossDb;
				}

				avgIteration++;
#pragma endregion
			}	// End - avgIteration

#pragma region Averaging and Saving Test Result
			/*----------------------------------*
			 * Averaging and Saving Test Result *
			 *----------------------------------*/
			if ( (ERR_OK==err) && captureOK && analysisOK )
			{
				// Average Power test result
				err = ::AverageTestResult(&powerAvEachBurst[0], avgIteration, LOG_10, l_txVerifyLEReturn.POWER_AVERAGE_DBM, dummyMax, dummyMin);
				// Peak Power test result
				err = ::AverageTestResult(&powerPkEachBurst[0], avgIteration, LOG_10, dummyMax, l_txVerifyLEReturn.POWER_PEAK_DBM, dummyMin);
			}
			else
			{
				// do nothing
			}
#pragma endregion

			/*-----------*
			 *  Tx Stop  *
			 *-----------*/
			err = vDUT_Run(g_BT_Dut, "TX_STOP");
			if ( ERR_OK!=err )
			{	// Check if vDUT returns "ERROR_MESSAGE" or not; if "Yes", then the error message must be handled and returned to the upper layer
				if ( ERR_OK==err )	// Get "ERROR_MESSAGE" from vDUT
				{
					err = -1;	// indicates that there is an error
					LogReturnMessage(logMessage, MAX_BUFFER_SIZE, LOGGER_ERROR, vDutErrorMsg);
					throw logMessage;
				}
				else	// Return generic error message to the upper layer
				{
					LogReturnMessage(logMessage, MAX_BUFFER_SIZE, LOGGER_ERROR, "[BT] vDUT_Run(TX_STOP) return error.\n");
					throw logMessage;
				}
			}
			else
			{
				vDutActived = false;
			}

		}
		else	// otherwise, perform Bluetooth LE analysis
		{
			/*--------------------------------*
			 * Start "while" loop for average *
			 *--------------------------------*/
			avgIteration = 0;
			while ( avgIteration<g_BTGlobalSettingParam.TX_LE_AVERAGE )
			{
				analysisOK = false;
				captureOK  = false;

				// Perform normal VSA capture
				err = LP_VsaDataCapture( samplingTimeUs/1000000, g_BTGlobalSettingParam.VSA_TRIGGER_TYPE );
				if( ERR_OK!=err )	// capture is failed
				{
					// Fail Capture
					LogReturnMessage(logMessage, MAX_BUFFER_SIZE, LOGGER_ERROR, "[BT] Fail to capture signal at %d MHz.\n", l_txVerifyLEParam.FREQ_MHZ);
					throw logMessage;
				}
				else
				{
					LogReturnMessage(logMessage, MAX_BUFFER_SIZE, LOGGER_INFORMATION, "[BT] LP_VsaDataCapture() at %d MHz return OK.\n", l_txVerifyLEParam.FREQ_MHZ);
				}

				/*--------------*
				 *  Capture OK  *
				 *--------------*/
				captureOK = true;
				if (1==g_BTGlobalSettingParam.VSA_SAVE_CAPTURE_ALWAYS)
				{
					// TODO: must give a warning that VSA_SAVE_CAPTURE_ALWAYS is ON
					sprintf_s(sigFileNameBuffer, MAX_BUFFER_SIZE, "%s_%d_%s", "BT_TX_LE_PRBS9_SaveAlways", l_txVerifyLEParam.FREQ_MHZ, l_txVerifyLEParam.PACKET_TYPE);
					BTSaveSigFile(sigFileNameBuffer);
				}
				else
				{
					// do nothing
				}

				/*------------------------------*
				 *  Perform Bluetooth analysis  *
				 *------------------------------*/
				err = LP_AnalyzeBluetooth( 4, "AllPlus" );		// 0 (auto), or 1, 2, 3, 4 (LE);
				if (ERR_OK!=err)
				{	// Fail Analysis, thus save capture (Signal File) for debug
					sprintf_s(sigFileNameBuffer, MAX_BUFFER_SIZE, "%s_%d_%s", "BT_TX_LE_AnalysisFailed", l_txVerifyLEParam.FREQ_MHZ, l_txVerifyLEParam.PACKET_TYPE);
					BTSaveSigFile(sigFileNameBuffer);
					LogReturnMessage(logMessage, MAX_BUFFER_SIZE, LOGGER_ERROR, "[BT] LP_AnalyzeBluetooth() return error.\n");
				}
				else
				{
					// do nothing
				}

#pragma region Retrieve analysis Results
				/*-----------------------------*
				 *  Retrieve analysis results  *
				 *-----------------------------*/
				analysisOK = true;

				if ( 1==LP_GetScalarMeasurement("leValid",0) )
				{
					// powerAvEachBurst
					powerAvEachBurst[avgIteration] = LP_GetScalarMeasurement("P_av_each_burst", 0);
					powerAvEachBurst[avgIteration] = 10 * log10(powerAvEachBurst[avgIteration]);
					if ( -99.00 >= powerAvEachBurst[avgIteration] )
					{
						analysisOK = false;
						l_txVerifyLEReturn.POWER_AVERAGE_DBM = NA_NUMBER;
						sprintf_s(sigFileNameBuffer, MAX_BUFFER_SIZE, "%s_%d_%s", "BT_TX_LE_AnalysisFailed", l_txVerifyLEParam.FREQ_MHZ, l_txVerifyLEParam.PACKET_TYPE);
						BTSaveSigFile(sigFileNameBuffer);
						err = -1;
						LogReturnMessage(logMessage, MAX_BUFFER_SIZE, LOGGER_ERROR, "LP_GetScalarMeasurement(P_av_each_burst) return error.\n");
						throw logMessage;
					}
					else
					{
						powerAvEachBurst[avgIteration] = powerAvEachBurst[avgIteration] + cableLossDb;
					}
					// powerPkEachBurst
					powerPkEachBurst[avgIteration] = LP_GetScalarMeasurement("P_pk_each_burst", 0);
					powerPkEachBurst[avgIteration] = 10 * log10(powerPkEachBurst[avgIteration]);
					if ( -99.00 >= powerPkEachBurst[avgIteration] )
					{
						analysisOK = false;
						l_txVerifyLEReturn.POWER_PEAK_DBM = NA_NUMBER;
						sprintf_s(sigFileNameBuffer, MAX_BUFFER_SIZE, "%s_%d_%s", "BT_TX_LE_AnalysisFailed", l_txVerifyLEParam.FREQ_MHZ, l_txVerifyLEParam.PACKET_TYPE);
						BTSaveSigFile(sigFileNameBuffer);
						err = -1;
						LogReturnMessage(logMessage, MAX_BUFFER_SIZE, LOGGER_ERROR, "LP_GetScalarMeasurement(P_pk_each_burst) return error.\n");
						throw logMessage;
					}
					else
					{
						powerPkEachBurst[avgIteration] = powerPkEachBurst[avgIteration] + cableLossDb;
					}

					//Map the LE datarate index to actual datarate, 1
					if ( 4 == LP_GetScalarMeasurement("dataRateDetect",0))
					{
						l_txVerifyLEReturn.DATA_RATE_DETECT	 = 1;
					}
					else
					{
						l_txVerifyLEReturn.DATA_RATE_DETECT	 = LP_GetScalarMeasurement("dataRateDetect",0);
					}

					if (1 != LP_GetScalarMeasurement("leIsCrcOk", 0))
					{
						l_txVerifyLEReturn.CRC_OK = 0;
					}
					else
					{
						//do nothing
					}
					LeFreqOffset[avgIteration]					 = LP_GetScalarMeasurement("leFreqOffset",  0) / 1000;

					if ( 1==LP_GetScalarMeasurement("acpErrValid",0) )
					{
						for (int i=0;i<=10;i++)
						{
							// maxPowerAcpDbm
							maxPowerAcpDbm[i][avgIteration]			 = LP_GetScalarMeasurement("maxPowerAcpDbm", i) + cableLossDb;
						}
					}
					else
					{
						analysisOK = false;
						sprintf_s(sigFileNameBuffer, MAX_BUFFER_SIZE, "%s_%d_%s", "BT_TX_LE_ResultsFailed", l_txVerifyLEParam.FREQ_MHZ, l_txVerifyLEParam.PACKET_TYPE);
						BTSaveSigFile(sigFileNameBuffer);
						err = -1;
						LogReturnMessage(logMessage, MAX_BUFFER_SIZE, LOGGER_ERROR, "BT TX LE (ACP) retrieve analysis results not valid.\n");
						throw logMessage;
					}
				}
				else
				{
					analysisOK = false;
					sprintf_s(sigFileNameBuffer, MAX_BUFFER_SIZE, "%s_%d_%s", "BT_TX_LE_ResultsFailed", l_txVerifyLEParam.FREQ_MHZ, l_txVerifyLEParam.PACKET_TYPE);
					BTSaveSigFile(sigFileNameBuffer);
					err = -1;
					LogReturnMessage(logMessage, MAX_BUFFER_SIZE, LOGGER_ERROR, "BT TX LE retrieve analysis results not valid.\n");
					throw logMessage;
				}

				avgIteration++;
#pragma endregion
			}	// End - avgIteration

#pragma region Averaging and Saving Test Result
			/*----------------------------------*
			 * Averaging and Saving Test Result *
			 *----------------------------------*/
			if ( (ERR_OK==err) && captureOK && analysisOK )
			{
				// Average Power test result
				err = ::AverageTestResult(&powerAvEachBurst[0], avgIteration, LOG_10, l_txVerifyLEReturn.POWER_AVERAGE_DBM, dummyMax, dummyMin);
				// Peak Power test result
				err = ::AverageTestResult(&powerPkEachBurst[0], avgIteration, LOG_10, dummyMax, l_txVerifyLEReturn.POWER_PEAK_DBM, dummyMin);
				// Bandwidth_20dB test result
				//				err = ::AverageTestResult(&bandwidth20dB[0], avgIteration, Linear, l_txVerifyLEReturn.BANDWIDTH_20DB, dummyMax, dummyMin);
				//Initial Carrier Frequency Tolerance
				err = ::AverageTestResult(&LeFreqOffset[0], avgIteration, Linear, l_txVerifyLEReturn.FREQ_OFFSET, dummyMax, dummyMin);

				// ACP
				for (int i=0;i<=10;i++)
				{
					err = ::AverageTestResult(&maxPowerAcpDbm[i][0], avgIteration, LOG_10, l_txVerifyLEReturn.MAX_POWER_ACP_DBM[i], dummyMax, dummyMin);
				}
			}
			else
			{
				// do nothing
			}
#pragma endregion

			/*-----------*
			 *  Tx Stop  *
			 *-----------*/
			err = vDUT_Run(g_BT_Dut, "TX_STOP");
			if ( ERR_OK!=err )
			{	// Check if vDUT returns "ERROR_MESSAGE" or not; if "Yes", then the error message must be handled and returned to the upper layer
				err = vDUT_GetStringReturn(g_BT_Dut, "ERROR_MESSAGE", vDutErrorMsg, MAX_BUFFER_SIZE);
				if ( ERR_OK==err )	// Get "ERROR_MESSAGE" from vDUT
				{
					err = -1;	// indicates that there is an "Error"
					LogReturnMessage(logMessage, MAX_BUFFER_SIZE, LOGGER_ERROR, vDutErrorMsg);
					throw logMessage;
				}
				else	// Return error message
				{
					LogReturnMessage(logMessage, MAX_BUFFER_SIZE, LOGGER_ERROR, "[BT] vDUT_Run(TX_STOP) return error.\n");
					throw logMessage;
				}
			}
			else
			{
				vDutActived = false;
			}

			if ( 1==l_txVerifyLEParam.TRANSMIT_0XF0_SEQUENCE_FOR_DELTA_F1_AVG )
			{
#pragma region Configure DUT to transmit
				/*---------------------------------------------------*
				 * Configure DUT to transmit - 11110000 data pattern *
				 *---------------------------------------------------*/
				// Set DUT RF frequency, tx power, data rate
				// And clear vDUT parameters at beginning.
				vDUT_ClearParameters(g_BT_Dut);

				vDUT_AddIntegerParameter(g_BT_Dut, "FREQ_MHZ",			l_txVerifyLEParam.FREQ_MHZ);
				vDUT_AddIntegerParameter(g_BT_Dut, "MODULATION_TYPE",   BT_LE_PATTERN_F0);	// HEX_F0=1: 11110000
				vDUT_AddIntegerParameter(g_BT_Dut, "LOGIC_CHANNEL",     1);	//{0x01, "ACL Basic"}
				vDUT_AddStringParameter (g_BT_Dut, "PACKET_TYPE",	    l_txVerifyLEParam.PACKET_TYPE);
				// Check packet length
				if (0==l_txVerifyLEParam.PACKET_LENGTH)
				{
					GetPacketLength("PER", "PACKETS_LENGTH", l_txVerifyLEParam.PACKET_TYPE, &packetLength);
				}
				else
				{
					packetLength = l_txVerifyLEParam.PACKET_LENGTH;
				}
				vDUT_AddIntegerParameter(g_BT_Dut, "PACKET_LENGTH",     packetLength);
				vDUT_AddIntegerParameter(g_BT_Dut, "TX_POWER_LEVEL",	l_txVerifyLEParam.TX_POWER_LEVEL);
				vDUT_AddDoubleParameter(g_BT_Dut, "EXPECTED_TX_POWER_DBM", l_txVerifyLEParam.EXPECTED_TX_POWER_DBM);

				err = vDUT_Run(g_BT_Dut, "TX_START");
				if ( ERR_OK!=err )
				{	// Check if vDUT returns "ERROR_MESSAGE" or not; if "Yes", then the error message must be handled and returned to the upper layer.
					vDutActived = false;
					err = vDUT_GetStringReturn(g_BT_Dut, "ERROR_MESSAGE", vDutErrorMsg, MAX_BUFFER_SIZE);
					if ( ERR_OK==err )	// Get "ERROR_MESSAGE" from vDUT
					{
						err = -1;	// Indicates that there is an "Error"
						LogReturnMessage(logMessage, MAX_BUFFER_SIZE, LOGGER_ERROR, vDutErrorMsg);
						throw logMessage;
					}
					else	// Returns error message
					{
						LogReturnMessage(logMessage, MAX_BUFFER_SIZE, LOGGER_ERROR, "[BT] vDUT_Run(TX_START) return error.\n");
						throw logMessage;
					}
				}
				else
				{
					vDutActived = true;
				}

				// Delay for DUT settle
				if (0!=g_BTGlobalSettingParam.DUT_TX_SETTLE_TIME_MS)
				{
					Sleep(g_BTGlobalSettingParam.DUT_TX_SETTLE_TIME_MS);
				}
				else
				{
					// do nothing
				}
#pragma endregion

				/*--------------------------------*
				 * Start "while" loop for average *
				 *--------------------------------*/
				avgIteration = 0;
				while ( avgIteration<g_BTGlobalSettingParam.TX_LE_AVERAGE )
				{
					analysisOK = false;
					captureOK  = false;

					/*----------------------------*
					 * Perform VSA capture *
					 *----------------------------*/
					err = LP_VsaDataCapture( samplingTimeUs/1000000, g_BTGlobalSettingParam.VSA_TRIGGER_TYPE );
					if( ERR_OK!=err )	// capture is failed
					{
						// Fail Capture
						LogReturnMessage(logMessage, MAX_BUFFER_SIZE, LOGGER_ERROR, "[BT] Fail to capture signal at %d MHz.\n", l_txVerifyLEParam.FREQ_MHZ);
						throw logMessage;
					}
					else
					{
						LogReturnMessage(logMessage, MAX_BUFFER_SIZE, LOGGER_INFORMATION, "[BT] LP_VsaDataCapture() at %d MHz return OK.\n", l_txVerifyLEParam.FREQ_MHZ);
					}

					/*--------------*
					 *  Capture OK  *
					 *--------------*/
					captureOK = true;
					if (1==g_BTGlobalSettingParam.VSA_SAVE_CAPTURE_ALWAYS)
					{
						// TODO: must give a warning that VSA_SAVE_CAPTURE_ALWAYS is ON
						sprintf_s(sigFileNameBuffer, MAX_BUFFER_SIZE, "%s_%d_%s", "BT_TX_LE_0xF0_SaveAlways", l_txVerifyLEParam.FREQ_MHZ, l_txVerifyLEParam.PACKET_TYPE);
						BTSaveSigFile(sigFileNameBuffer);
					}
					else
					{
						// do nothing
					}

					/*------------------------------*
					 *  Perform Bluetooth analysis  *
					 *------------------------------*/
					err = LP_AnalyzeBluetooth( 4 );		// 0 (auto), or 1, 2, 3, 4 (LE)
					if (ERR_OK!=err)
					{	// Fail Analysis, thus save capture (Signal File) for debug
						sprintf_s(sigFileNameBuffer, MAX_BUFFER_SIZE, "%s_%d_%s", "BT_TX_LE_AnalysisFailed", l_txVerifyLEParam.FREQ_MHZ, l_txVerifyLEParam.PACKET_TYPE);
						BTSaveSigFile(sigFileNameBuffer);
						LogReturnMessage(logMessage, MAX_BUFFER_SIZE, LOGGER_ERROR, "[BT] LP_AnalyzeBluetooth() return error.\n");
						throw logMessage;
					}
					else
					{
						// do nothing
					}


#pragma region Retrieve analysis Results
					/*-----------------------------*
					 *  Retrieve analysis results  *
					 *-----------------------------*/
					analysisOK = true;

					if ( 1==LP_GetScalarMeasurement("leValid",0) )
					{
						if (1 != LP_GetScalarMeasurement("leIsCrcOk", 0))
						{
							l_txVerifyLEReturn.CRC_OK = 0;
						}
						else
						{
							//do nothing
						}
						leDeltaF1Avg[avgIteration]		    = LP_GetScalarMeasurement("leDeltaF1Avg", 0) / 1000;	// Requires 00001111 data pattern
					}
					else
					{
						analysisOK = false;
						sprintf_s(sigFileNameBuffer, MAX_BUFFER_SIZE, "%s_%d_%s", "BT_TX_LE_ResultsFailed", l_txVerifyLEParam.FREQ_MHZ, l_txVerifyLEParam.PACKET_TYPE);
						BTSaveSigFile(sigFileNameBuffer);
						err = -1;
						LogReturnMessage(logMessage, MAX_BUFFER_SIZE, LOGGER_ERROR, "BT TX LE retrieve analysis results not valid.\n");
						throw logMessage;
					}

					avgIteration++;
#pragma endregion
				}	// End - avgIteration

#pragma region Averaging and Saving Test Result
				/*----------------------------------*
				 * Averaging and Saving Test Result *
				 *----------------------------------*/
				if ( (ERR_OK==err) && captureOK && analysisOK )
				{
					err = ::AverageTestResult(&leDeltaF1Avg[0], avgIteration, Linear, l_txVerifyLEReturn.DELTA_F1_AVERAGE, dummyMax, dummyMin);
				}
				else
				{
					// do nothing
				}
#pragma endregion

				/*-----------*
				 *  Tx Stop  *
				 *-----------*/
				err = vDUT_Run(g_BT_Dut, "TX_STOP");
				if ( ERR_OK!=err )
				{	// Check if vDUT returns "ERROR_MESSAGE" or not; if "Yes", then the error message must be handled and returned to the upper layer.
					err = vDUT_GetStringReturn(g_BT_Dut, "ERROR_MESSAGE", vDutErrorMsg, MAX_BUFFER_SIZE);
					if ( ERR_OK==err )	// Get "ERROR_MESSAGE" from vDut
					{
						err = -1;	// indicates that there is an "Error"
						LogReturnMessage(logMessage, MAX_BUFFER_SIZE, LOGGER_ERROR, vDutErrorMsg);
						throw logMessage;
					}
					else	// Return error message
					{
						LogReturnMessage(logMessage, MAX_BUFFER_SIZE, LOGGER_ERROR, "[BT] vDUT_Run(TX_STOP) return error.\n");
						throw logMessage;
					}
				}
				else
				{
					vDutActived = false;
				}
			}
			else
			{
				// do nothing
			}

#pragma region Configure DUT to transmit
			/*------------------------------------------------------------*
			 * Configure DUT to transmit - alternating data pattern (1010)*
			 *------------------------------------------------------------*/
			// Set DUT RF frequency, tx power, data rate
			// And clear vDUT parameters at the beginning.
			vDUT_ClearParameters(g_BT_Dut);

			vDUT_AddIntegerParameter(g_BT_Dut, "FREQ_MHZ",			l_txVerifyLEParam.FREQ_MHZ);
			vDUT_AddIntegerParameter(g_BT_Dut, "MODULATION_TYPE",   BT_LE_PATTERN_AA);	// HEX_A=2: 1010
			vDUT_AddIntegerParameter(g_BT_Dut, "LOGIC_CHANNEL",     1);	//{0x01, "ACL Basic"}
			vDUT_AddStringParameter (g_BT_Dut, "PACKET_TYPE",	    l_txVerifyLEParam.PACKET_TYPE);
			// Check packet length
			if (0==l_txVerifyLEParam.PACKET_LENGTH)
			{
				GetPacketLength("PER", "PACKETS_LENGTH", l_txVerifyLEParam.PACKET_TYPE, &packetLength);
			}
			else
			{
				packetLength = l_txVerifyLEParam.PACKET_LENGTH;
			}
			vDUT_AddIntegerParameter(g_BT_Dut, "PACKET_LENGTH",     packetLength);
			vDUT_AddIntegerParameter(g_BT_Dut, "TX_POWER_LEVEL",	l_txVerifyLEParam.TX_POWER_LEVEL);
			vDUT_AddDoubleParameter(g_BT_Dut, "EXPECTED_TX_POWER_DBM", l_txVerifyLEParam.EXPECTED_TX_POWER_DBM);

			err = vDUT_Run(g_BT_Dut, "TX_START");
			if ( ERR_OK!=err )
			{
				vDutActived = false;
				// Check if vDUT returns "ERROR_MESSAGE" or not; if "Yes", then the error message must be handled and returned to the upper layer.
				err = vDUT_GetStringReturn(g_BT_Dut, "ERROR_MESSAGE", vDutErrorMsg, MAX_BUFFER_SIZE);
				if ( ERR_OK==err )	// Get "ERROR_MESSAGE" from vDUT
				{
					err = -1;	//indicates that there is an "Error"
					LogReturnMessage(logMessage, MAX_BUFFER_SIZE, LOGGER_ERROR, vDutErrorMsg);
					throw logMessage;
				}
				else	// Return error message
				{
					LogReturnMessage(logMessage, MAX_BUFFER_SIZE, LOGGER_ERROR, "[BT] vDUT_Run(TX_START) return error.\n");
					throw logMessage;
				}
			}
			else
			{
				vDutActived = true;
			}

			// Delay for DUT settle
			if (0!=g_BTGlobalSettingParam.DUT_TX_SETTLE_TIME_MS)
			{
				Sleep(g_BTGlobalSettingParam.DUT_TX_SETTLE_TIME_MS);
			}
			else
			{
				// do nothing
			}
#pragma endregion

			/*--------------------------------*
			 * Start "while" loop for average *
			 *--------------------------------*/
			avgIteration = 0;
			while ( avgIteration<g_BTGlobalSettingParam.TX_LE_AVERAGE )
			{
				analysisOK = false;
				captureOK  = false;

				/*----------------------------*
				 * Perform normal VSA capture *
				 *----------------------------*/
				err = LP_VsaDataCapture( samplingTimeUs/1000000, g_BTGlobalSettingParam.VSA_TRIGGER_TYPE );
				if( ERR_OK!=err )	// capture is failed
				{
					// Fail Capture
					LogReturnMessage(logMessage, MAX_BUFFER_SIZE, LOGGER_ERROR, "[BT] Fail to capture signal at %d MHz.\n", l_txVerifyLEParam.FREQ_MHZ);
					throw logMessage;
				}
				else
				{
					LogReturnMessage(logMessage, MAX_BUFFER_SIZE, LOGGER_INFORMATION, "[BT] LP_VsaDataCapture() at %d MHz return OK.\n", l_txVerifyLEParam.FREQ_MHZ);
				}

				/*--------------*
				 *  Capture OK  *
				 *--------------*/
				captureOK = true;
				if (1==g_BTGlobalSettingParam.VSA_SAVE_CAPTURE_ALWAYS)
				{
					// TODO: must give a warning that VSA_SAVE_CAPTURE_ALWAYS is ON
					sprintf_s(sigFileNameBuffer, MAX_BUFFER_SIZE, "%s_%d_%s", "BT_TX_LE_0xAA_SaveAlways", l_txVerifyLEParam.FREQ_MHZ, l_txVerifyLEParam.PACKET_TYPE);
					BTSaveSigFile(sigFileNameBuffer);
				}
				else
				{
					// do nothing
				}

				/*------------------------------*
				 *  Perform Bluetooth analysis  *
				 *------------------------------*/
				err = LP_AnalyzeBluetooth( 4 );		// 0 (auto), or 1, 2, 3, 4 (LE)
				if (ERR_OK!=err)
				{	// Fail Analysis, thus save capture (Signal File) for debug
					sprintf_s(sigFileNameBuffer, MAX_BUFFER_SIZE, "%s_%d_%s", "BT_TX_LE_AnalysisFailed", l_txVerifyLEParam.FREQ_MHZ, l_txVerifyLEParam.PACKET_TYPE);
					BTSaveSigFile(sigFileNameBuffer);
					LogReturnMessage(logMessage, MAX_BUFFER_SIZE, LOGGER_ERROR, "[BT] LP_AnalyzeBluetooth() return error.\n");
					throw logMessage;
				}
				else
				{
					// do nothing
				}

#pragma region Retrieve analysis Results
				/*-----------------------------*
				 *  Retrieve analysis results  *
				 *-----------------------------*/
				analysisOK = true;

				if ( 1==LP_GetScalarMeasurement("valid",0) )
				{
					//Get result "freq_est" while payload is PRBS9
					//LeFreqOffset[avgIteration]			= LP_GetScalarMeasurement("leFreqOffset",  0) / 1000;

					if (1 != LP_GetScalarMeasurement("leIsCrcOk", 0))
					{
						l_txVerifyLEReturn.CRC_OK = 0;
					}
					else
					{
						//do nothing
					}

					leDeltaF2Max[avgIteration]		= LP_GetScalarMeasurement("leDeltaF2Max",    0) / 1000;	// Requires alternating data pattern
					leDeltaF2Avg[avgIteration]	    = LP_GetScalarMeasurement("leDeltaF2Avg",0) / 1000;	// Requires alternating data pattern
					leFnMax[avgIteration]	        = LP_GetScalarMeasurement("leFnMax",0) / 1000;
					leDeltaF0FnMax[avgIteration]	= LP_GetScalarMeasurement("leDeltaF0FnMax" ,0) / 1000;
					leDeltaF1F0[avgIteration]	    = LP_GetScalarMeasurement("leDeltaF1F0" ,0) / 1000;
					leDeltaFnFn_5Max[avgIteration]	= LP_GetScalarMeasurement("leDeltaFnFn_5Max" ,0) / 1000;
					leFreqDevSyncAvg[avgIteration]	= LP_GetScalarMeasurement("leFreqDevSyncAv" ,0) / 1000;
				}
				else
				{
					analysisOK = false;
					sprintf_s(sigFileNameBuffer, MAX_BUFFER_SIZE, "%s_%d_%s", "BT_TX_LE_ResultsFailed", l_txVerifyLEParam.FREQ_MHZ, l_txVerifyLEParam.PACKET_TYPE);
					BTSaveSigFile(sigFileNameBuffer);
					err = -1;
					LogReturnMessage(logMessage, MAX_BUFFER_SIZE, LOGGER_ERROR, "BT TX LE retrieve analysis results not valid.\n");
					throw logMessage;
				}
				avgIteration++;
#pragma endregion
			}	// End - avgIteration

#pragma region Averaging and Saving Test Result
			/*----------------------------------*
			 * Averaging and Saving Test Result *
			 *----------------------------------*/
			if ( (ERR_OK==err) && captureOK && analysisOK )
			{
				//Get result "LeFreqOffset" while payload is PRBS9
				//err = ::AverageTestResult(&LeFreqOffset[0], avgIteration, Linear, l_txVerifyLEReturn.FREQ_OFFSET, dummyMax, dummyMin);

				err = ::AverageTestResult(&leDeltaF2Max[0], avgIteration, Linear, l_txVerifyLEReturn.DELTA_F2_MAX, dummyMax, dummyMin);
				err = ::AverageTestResult(&leDeltaF2Avg[0], avgIteration, Linear, l_txVerifyLEReturn.DELTA_F2_AVERAGE, dummyMax, dummyMin);
				err = ::AverageTestResult(&leFnMax[0], avgIteration, Linear, l_txVerifyLEReturn.Fn_MAX, dummyMax, dummyMin);
				err = ::AverageTestResult(&leDeltaF0FnMax[0], avgIteration, Linear, l_txVerifyLEReturn.DELTA_F0_Fn_MAX, dummyMax, dummyMin);
				err = ::AverageTestResult(&leDeltaF1F0[0], avgIteration, Linear, l_txVerifyLEReturn.DELTA_F1_F0, dummyMax, dummyMin);
				err = ::AverageTestResult(&leDeltaFnFn_5Max[0], avgIteration, Linear, l_txVerifyLEReturn.DELTA_Fn_Fn5_MAX, dummyMax, dummyMin);
				err = ::AverageTestResult(&leFreqDevSyncAvg[0], avgIteration, Linear, l_txVerifyLEReturn.FREQ_DEV_SYNC_AVG, dummyMax, dummyMin);
			}
			else
			{
				// do nothing
			}
#pragma endregion

			/*-----------*
			 *  Tx Stop  *
			 *-----------*/
			err = vDUT_Run(g_BT_Dut, "TX_STOP");
			if ( ERR_OK!=err )
			{	// Check if vDUT returns "ERROR_MESSAGE" or not; if "Yes", then the error message must be handled and returned to the upper layer.
				err = vDUT_GetStringReturn(g_BT_Dut, "ERROR_MESSAGE", vDutErrorMsg, MAX_BUFFER_SIZE);
				if ( ERR_OK==err )	// Get "ERROR_MESSAGE" from vDUT
				{
					err = -1;	// indicates that there is an "Error"
					LogReturnMessage(logMessage, MAX_BUFFER_SIZE, LOGGER_ERROR, vDutErrorMsg);
					throw logMessage;
				}
				else	// return error message
				{
					LogReturnMessage(logMessage, MAX_BUFFER_SIZE, LOGGER_ERROR, "[BT] vDUT_Run(TX_STOP) return error.\n");
					throw logMessage;
				}
			}
			else
			{
				vDutActived = false;
			}

		}

		/*-----------------------*
		 *  Return Test Results  *
		 *-----------------------*/
		if ( ERR_OK==err && captureOK && analysisOK )
		{
			// Return Path Loss (dB)
			l_txVerifyLEReturn.CABLE_LOSS_DB = l_txVerifyLEParam.CABLE_LOSS_DB;

			// DELTA_F2_AV / DELTA_F1_AV >= 0.8
			if (l_txVerifyLEReturn.DELTA_F1_AVERAGE!=(NA_NUMBER/1000))
			{
				if (l_txVerifyLEReturn.DELTA_F2_AVERAGE!=(NA_NUMBER/1000))
					l_txVerifyLEReturn.DELTA_F2_F1_AV_RATIO = l_txVerifyLEReturn.DELTA_F2_AVERAGE / l_txVerifyLEReturn.DELTA_F1_AVERAGE;

			}
			else
			{
				// do nothing
			}

			sprintf_s(l_txVerifyLEReturn.ERROR_MESSAGE, MAX_BUFFER_SIZE, "[Info] Function completed.\n");
			ReturnTestResults(l_txVerifyLEReturnMap);
		}
		else
		{
			// do nothing
		}
	}
	catch(char *msg)
	{
		ReturnErrorMessage(l_txVerifyLEReturn.ERROR_MESSAGE, msg);
	}
	catch(...)
	{
		ReturnErrorMessage(l_txVerifyLEReturn.ERROR_MESSAGE, "[BT] Unknown Error!\n");
		err = -1;
	}

	// This is a special case and happens only when certain errors occur before the TX_STOP.
	// This is handled by error handling, but TX_STOP must be handled manually.

	if ( vDutActived )
	{
		vDUT_Run(g_BT_Dut, "TX_STOP");
	}
	else
	{
		// do nothing
	}

	// Free memory
	powerAvEachBurst.clear();
	powerPkEachBurst.clear();
	leDeltaF1Avg.clear();
	LeFreqOffset.clear();
	leDeltaF2Max.clear();
	leDeltaF2Avg.clear();
	leFnMax.clear();
	leDeltaF0FnMax.clear();
	leDeltaF1F0.clear();
	leDeltaFnFn_5Max.clear();
	leFreqDevSyncAvg.clear();

	return err;
}

int InitializeTXVerifyLEContainers(void)
{
	/*------------------*
	 * Input Parameters  *
	 *------------------*/
	l_txVerifyLEParamMap.clear();

	BT_SETTING_STRUCT setting;

	l_txVerifyLEParam.ANALYZE_POWER_ONLY = 0;
	setting.type = BT_SETTING_TYPE_INTEGER;
	if (sizeof(int)==sizeof(l_txVerifyLEParam.ANALYZE_POWER_ONLY))    // Type_Checking
	{
		setting.value       = (void*)&l_txVerifyLEParam.ANALYZE_POWER_ONLY;
		setting.unit        = "";
		setting.helpText    = "The index to indicate ANALYZE_POWER_ONLY, default=0, 0: OFF, 1: ON ";
		l_txVerifyLEParamMap.insert( pair<string,BT_SETTING_STRUCT>("ANALYZE_POWER_ONLY", setting) );
	}
	else
	{
		printf("Parameter Type Error!\n");
		exit(1);
	}

	l_txVerifyLEParam.TRANSMIT_0XF0_SEQUENCE_FOR_DELTA_F1_AVG = 1;
	setting.type = BT_SETTING_TYPE_INTEGER;
	if (sizeof(int)==sizeof(l_txVerifyLEParam.TRANSMIT_0XF0_SEQUENCE_FOR_DELTA_F1_AVG))    // Type_Checking
	{
		setting.value       = (void*)&l_txVerifyLEParam.TRANSMIT_0XF0_SEQUENCE_FOR_DELTA_F1_AVG;
		setting.unit        = "";
		setting.helpText    = "The index to transmit 11110000 sequence for delta_f1_avg, default=1, 0: OFF, 1: ON ";
		l_txVerifyLEParamMap.insert( pair<string,BT_SETTING_STRUCT>("TRANSMIT_0XF0_SEQUENCE_FOR_DELTA_F1_AVG", setting) );
	}
	else
	{
		printf("Parameter Type Error!\n");
		exit(1);
	}

	l_txVerifyLEParam.FREQ_MHZ = 2402;
	setting.type = BT_SETTING_TYPE_INTEGER;
	if (sizeof(int)==sizeof(l_txVerifyLEParam.FREQ_MHZ))    // Type_Checking
	{
		setting.value       = (void*)&l_txVerifyLEParam.FREQ_MHZ;
		setting.unit        = "MHz";
		setting.helpText    = "Channel center frequency in MHz, 2 MHz per step";
		l_txVerifyLEParamMap.insert( pair<string,BT_SETTING_STRUCT>("FREQ_MHZ", setting) );
	}
	else
	{
		printf("Parameter Type Error!\n");
		exit(1);
	}

	strcpy_s(l_txVerifyLEParam.PACKET_TYPE, MAX_BUFFER_SIZE, "1LE");
	setting.type = BT_SETTING_TYPE_STRING;
	if (MAX_BUFFER_SIZE==sizeof(l_txVerifyLEParam.PACKET_TYPE))    // Type_Checking
	{
		setting.value       = (void*)l_txVerifyLEParam.PACKET_TYPE;
		setting.unit        = "";
		setting.helpText    = "Sets the packet type, only allowed value is 1LE. Default = 1LE";
		l_txVerifyLEParamMap.insert( pair<string,BT_SETTING_STRUCT>("PACKET_TYPE", setting) );
	}
	else
	{
		printf("Parameter Type Error!\n");
		exit(1);
	}

	l_txVerifyLEParam.PACKET_LENGTH = 0;
	setting.type = BT_SETTING_TYPE_INTEGER;
	if (sizeof(int)==sizeof(l_txVerifyLEParam.PACKET_LENGTH))    // Type_Checking
	{
		setting.value       = (void*)&l_txVerifyLEParam.PACKET_LENGTH;
		setting.unit        = "";
		setting.helpText    = "The number of octets in one packet to verify LE. Maximum value is 37. Default = 0, means using default global setting value (for LE PER test).";
		l_txVerifyLEParamMap.insert( pair<string,BT_SETTING_STRUCT>("PACKET_LENGTH", setting) );
	}
	else
	{
		printf("Parameter Type Error!\n");
		exit(1);
	}

	l_txVerifyLEParam.TX_POWER_LEVEL = 0;
	setting.type = BT_SETTING_TYPE_INTEGER;
	if (sizeof(int)==sizeof(l_txVerifyLEParam.TX_POWER_LEVEL))    // Type_Checking
	{
		setting.value = (void*)&l_txVerifyLEParam.TX_POWER_LEVEL;
		setting.unit        = "Level";
		setting.helpText    = "Not Used! LE does not support power control.Reserved for future usage. Expected power level at DUT antenna port. Level can be 0, 1, 2, 3, 4, 5, 6 and 7 (MaxPower => MinPower, if supported.)";
		l_txVerifyLEParamMap.insert( pair<string,BT_SETTING_STRUCT>("TX_POWER_LEVEL", setting) );
	}
	else
	{
		printf("Parameter Type Error!\n");
		exit(1);
	}

	l_txVerifyLEParam.EXPECTED_TX_POWER_DBM = 10;
	setting.type = BT_SETTING_TYPE_DOUBLE;
	if (sizeof(double)==sizeof(l_txVerifyLEParam.EXPECTED_TX_POWER_DBM))    // Type_Checking
	{
		setting.value = (void*)&l_txVerifyLEParam.EXPECTED_TX_POWER_DBM;
		setting.unit        = "dBm";
		setting.helpText    = "Expected TX power dBm at TX_POWER_LEVEL. Default is 10 (dBm).";
		l_txVerifyLEParamMap.insert( pair<string,BT_SETTING_STRUCT>("EXPECTED_TX_POWER_DBM", setting) );
	}
	else
	{
		printf("Parameter Type Error!\n");
		exit(1);
	}

	l_txVerifyLEParam.CABLE_LOSS_DB = 0.0;
	setting.type = BT_SETTING_TYPE_DOUBLE;
	if (sizeof(double)==sizeof(l_txVerifyLEParam.CABLE_LOSS_DB))    // Type_Checking
	{
		setting.value       = (void*)&l_txVerifyLEParam.CABLE_LOSS_DB;
		setting.unit        = "dB";
		setting.helpText    = "Cable loss from the DUT antenna port to tester. Default = 0.0,  means using default global setting value.";
		l_txVerifyLEParamMap.insert( pair<string,BT_SETTING_STRUCT>("CABLE_LOSS_DB", setting) );
	}
	else
	{
		printf("Parameter Type Error!\n");
		exit(1);
	}

	l_txVerifyLEParam.SAMPLING_TIME_US = 0;
	setting.type = BT_SETTING_TYPE_DOUBLE;
	if (sizeof(double)==sizeof(l_txVerifyLEParam.SAMPLING_TIME_US))    // Type_Checking
	{
		setting.value = (void*)&l_txVerifyLEParam.SAMPLING_TIME_US;
		setting.unit        = "us";
		setting.helpText    = "Capture time in micro-seconds. Default = 0,  means using default global setting value.";
		l_txVerifyLEParamMap.insert( pair<string,BT_SETTING_STRUCT>("SAMPLING_TIME_US", setting) );
	}
	else
	{
		printf("Parameter Type Error!\n");
		exit(1);
	}

	/*----------------*
	 * Return Values: *
	 * ERROR_MESSAGE  *
	 *----------------*/
	l_txVerifyLEReturnMap.clear();

	// <Perform LP_AnalyzePower>
	l_txVerifyLEReturn.POWER_AVERAGE_DBM = NA_NUMBER;
	setting.type = BT_SETTING_TYPE_DOUBLE;
	if (sizeof(double)==sizeof(l_txVerifyLEReturn.POWER_AVERAGE_DBM))    // Type_Checking
	{
		setting.value = (void*)&l_txVerifyLEReturn.POWER_AVERAGE_DBM;
		setting.unit        = "dBm";
		setting.helpText    = "Average power in dBm.";
		l_txVerifyLEReturnMap.insert( pair<string,BT_SETTING_STRUCT>("POWER_AVERAGE_DBM", setting) );
	}
	else
	{
		printf("Parameter Type Error!\n");
		exit(1);
	}

	l_txVerifyLEReturn.POWER_PEAK_DBM = NA_NUMBER;
	setting.type = BT_SETTING_TYPE_DOUBLE;
	if (sizeof(double)==sizeof(l_txVerifyLEReturn.POWER_PEAK_DBM))    // Type_Checking
	{
		setting.value = (void*)&l_txVerifyLEReturn.POWER_PEAK_DBM;
		setting.unit        = "dBm";
		setting.helpText    = "Peak power in dBm.";
		l_txVerifyLEReturnMap.insert( pair<string,BT_SETTING_STRUCT>("POWER_PEAK_DBM", setting) );
	}
	else
	{
		printf("Parameter Type Error!\n");
		exit(1);
	}

	l_txVerifyLEReturn.TARGET_POWER_DBM = NA_NUMBER;
	setting.type = BT_SETTING_TYPE_DOUBLE;
	if (sizeof(double)==sizeof(l_txVerifyLEReturn.TARGET_POWER_DBM))    // Type_Checking
	{
		setting.value = (void*)&l_txVerifyLEReturn.TARGET_POWER_DBM;
		setting.unit        = "dBm";
		setting.helpText    = "Expected target power dBm at DUT antenna port.";
		l_txVerifyLEReturnMap.insert( pair<string,BT_SETTING_STRUCT>("TARGET_POWER_DBM", setting) );
	}
	else
	{
		printf("Parameter Type Error!\n");
		exit(1);
	}

	// <Perform LP_AnalyzeBluetooth>
	l_txVerifyLEReturn.DATA_RATE_DETECT = NA_NUMBER;
	setting.type = BT_SETTING_TYPE_DOUBLE;
	if (sizeof(double)==sizeof(l_txVerifyLEReturn.DATA_RATE_DETECT))    // Type_Checking
	{
		setting.value = (void*)&l_txVerifyLEReturn.DATA_RATE_DETECT;
		setting.unit        = "Mbps";
		setting.helpText    = "Bluetooth datarate, can be 1, 2, 3 Mbps or 4 (LE).";
		l_txVerifyLEReturnMap.insert( pair<string,BT_SETTING_STRUCT>("DATA_RATE_DETECT", setting) );
	}
	else
	{
		printf("Parameter Type Error!\n");
		exit(1);
	}

	l_txVerifyLEReturn.DELTA_F1_AVERAGE = NA_NUMBER;
	setting.type = BT_SETTING_TYPE_DOUBLE;
	if (sizeof(double)==sizeof(l_txVerifyLEReturn.DELTA_F1_AVERAGE))    // Type_Checking
	{
		setting.value = (void*)&l_txVerifyLEReturn.DELTA_F1_AVERAGE;
		setting.unit        = "kHz";
		setting.helpText    = "The measurement result for deltaF1Avg as specified in BLUETOOTH TEST SPECIFICATION Ver. 1.2/2.0/2.0 + EDR [vol 2] version 2.0.E.2. Requires 00001111 data pattern. Result in kHz.";
		l_txVerifyLEReturnMap.insert( pair<string,BT_SETTING_STRUCT>("DELTA_F1_AVERAGE", setting) );
	}
	else
	{
		printf("Parameter Type Error!\n");
		exit(1);
	}

	l_txVerifyLEReturn.DELTA_F2_MAX = NA_NUMBER;
	setting.type = BT_SETTING_TYPE_DOUBLE;
	if (sizeof(double)==sizeof(l_txVerifyLEReturn.DELTA_F2_MAX))    // Type_Checking
	{
		setting.value = (void*)&l_txVerifyLEReturn.DELTA_F2_MAX;
		setting.unit        = "kHz";
		setting.helpText    = "The measurement result for deltaF2Max as specified in BLUETOOTH TEST SPECIFICATION Ver. 1.2/2.0/2.0 + EDR [vol 2] version 2.0.E.2. Requires alternating data pattern. Result in kHz.";
		l_txVerifyLEReturnMap.insert( pair<string,BT_SETTING_STRUCT>("DELTA_F2_MAX", setting) );
	}
	else
	{
		printf("Parameter Type Error!\n");
		exit(1);
	}

	l_txVerifyLEReturn.DELTA_F2_AVERAGE = NA_NUMBER;
	setting.type = BT_SETTING_TYPE_DOUBLE;
	if (sizeof(double)==sizeof(l_txVerifyLEReturn.DELTA_F2_AVERAGE))    // Type_Checking
	{
		setting.value = (void*)&l_txVerifyLEReturn.DELTA_F2_AVERAGE;
		setting.unit        = "kHz";
		setting.helpText    = "The measurement result for deltaF2Avg as specified in BLUETOOTH TEST SPECIFICATION Ver. 1.2/2.0/2.0 + EDR [vol 2] version 2.0.E.2. Requires alternating data pattern. Result in kHz.";
		l_txVerifyLEReturnMap.insert( pair<string,BT_SETTING_STRUCT>("DELTA_F2_AVERAGE", setting) );
	}
	else
	{
		printf("Parameter Type Error!\n");
		exit(1);
	}

	l_txVerifyLEReturn.Fn_MAX = NA_NUMBER;
	setting.type = BT_SETTING_TYPE_DOUBLE;
	if (sizeof(double)==sizeof(l_txVerifyLEReturn.Fn_MAX))    // Type_Checking
	{
		setting.value = (void*)&l_txVerifyLEReturn.Fn_MAX;
		setting.unit        = "kHz";
		setting.helpText    = "Maximum value of the absolute value of LE Fn. Result in kHz.";
		l_txVerifyLEReturnMap.insert( pair<string,BT_SETTING_STRUCT>("Fn_MAX", setting) );
	}
	else
	{
		printf("Parameter Type Error!\n");
		exit(1);
	}

	l_txVerifyLEReturn.DELTA_F0_Fn_MAX = NA_NUMBER;
	setting.type = BT_SETTING_TYPE_DOUBLE;
	if (sizeof(double)==sizeof(l_txVerifyLEReturn.DELTA_F0_Fn_MAX))    // Type_Checking
	{
		setting.value = (void*)&l_txVerifyLEReturn.DELTA_F0_Fn_MAX;
		setting.unit        = "kHz";
		setting.helpText    = "Maximum value of |f0 - fn|, with n = 2, 3, ... Result in kHz.";
		l_txVerifyLEReturnMap.insert( pair<string,BT_SETTING_STRUCT>("DELTA_F0_Fn_MAX", setting) );
	}
	else
	{
		printf("Parameter Type Error!\n");
		exit(1);
	}

	l_txVerifyLEReturn.DELTA_F1_F0 = NA_NUMBER;
	setting.type = BT_SETTING_TYPE_DOUBLE;
	if (sizeof(double)==sizeof(l_txVerifyLEReturn.DELTA_F1_F0))    // Type_Checking
	{
		setting.value = (void*)&l_txVerifyLEReturn.DELTA_F1_F0;
		setting.unit        = "kHz";
		setting.helpText    = "Absolute value of f1 - f0. Result in kHz.";
		l_txVerifyLEReturnMap.insert( pair<string,BT_SETTING_STRUCT>("DELTA_F1_F0", setting) );
	}
	else
	{
		printf("Parameter Type Error!\n");
		exit(1);
	}

	l_txVerifyLEReturn.DELTA_Fn_Fn5_MAX = NA_NUMBER;
	setting.type = BT_SETTING_TYPE_DOUBLE;
	if (sizeof(double)==sizeof(l_txVerifyLEReturn.DELTA_Fn_Fn5_MAX))    // Type_Checking
	{
		setting.value = (void*)&l_txVerifyLEReturn.DELTA_Fn_Fn5_MAX;
		setting.unit        = "kHz";
		setting.helpText    = "Maximum value of |fn - fn-5|, with n = 6, 7, ... Result in kHz.";
		l_txVerifyLEReturnMap.insert( pair<string,BT_SETTING_STRUCT>("DELTA_Fn_Fn5_MAX", setting) );
	}
	else
	{
		printf("Parameter Type Error!\n");
		exit(1);
	}

	l_txVerifyLEReturn.FREQ_DEV_SYNC_AVG = NA_NUMBER;
	setting.type = BT_SETTING_TYPE_DOUBLE;
	if (sizeof(double)==sizeof(l_txVerifyLEReturn.FREQ_DEV_SYNC_AVG))    // Type_Checking
	{
		setting.value = (void*)&l_txVerifyLEReturn.FREQ_DEV_SYNC_AVG;
		setting.unit        = "kHz";
		setting.helpText    = "Average freq. deviation during synch. Measured at single sample per symbol after frequency offset during preamble has been removed. Result in kHz.";
		l_txVerifyLEReturnMap.insert( pair<string,BT_SETTING_STRUCT>("FREQ_DEV_SYNC_AVG", setting) );
	}
	else
	{
		printf("Parameter Type Error!\n");
		exit(1);
	}

	l_txVerifyLEReturn.DELTA_F2_F1_AV_RATIO = NA_NUMBER;
	setting.type = BT_SETTING_TYPE_DOUBLE;
	if (sizeof(double)==sizeof(l_txVerifyLEReturn.DELTA_F2_F1_AV_RATIO))    // Type_Checking
	{
		setting.value = (void*)&l_txVerifyLEReturn.DELTA_F2_F1_AV_RATIO;
		setting.unit        = "";
		setting.helpText    = "The measurement result for deltaF2Avg/deltaF1Avg, typically the ratio should be >= 0.8";
		l_txVerifyLEReturnMap.insert( pair<string,BT_SETTING_STRUCT>("DELTA_F2_F1_AV_RATIO", setting) );
	}
	else
	{
		printf("Parameter Type Error!\n");
		exit(1);
	}


	l_txVerifyLEReturn.FREQ_OFFSET = NA_NUMBER;
	setting.type = BT_SETTING_TYPE_DOUBLE;
	if (sizeof(double)==sizeof(l_txVerifyLEReturn.FREQ_OFFSET))    // Type_Checking
	{
		setting.value = (void*)&l_txVerifyLEReturn.FREQ_OFFSET;
		setting.unit        = "kHz";
		setting.helpText    = "Initial freq offset of each burst detected, in kHz.";
		l_txVerifyLEReturnMap.insert( pair<string,BT_SETTING_STRUCT>("FREQ_OFFSET", setting) );
	}
	else
	{
		printf("Parameter Type Error!\n");
		exit(1);
	}

	l_txVerifyLEReturn.CRC_OK = (int)NA_NUMBER;
	setting.type = BT_SETTING_TYPE_INTEGER;
	if (sizeof(int)==sizeof(l_txVerifyLEReturn.CRC_OK))    // Type_Checking
	{
		setting.value = (void*)&l_txVerifyLEReturn.CRC_OK;
		setting.unit        = "";
		setting.helpText    = "Reports whether CRC of LE packet is valid. 1 if CRC is valid, else 0./n Note: always 1 in ANALYZE_POWER_ONLY option";
		l_txVerifyLEReturnMap.insert( pair<string,BT_SETTING_STRUCT>("CRC_OK", setting) );
	}
	else
	{
		printf("Parameter Type Error!\n");
		exit(1);
	}

	for (int i=0;i<BT_ACP_SECTION;i++)
	{
		l_txVerifyLEReturn.MAX_POWER_ACP_DBM[i] = NA_NUMBER;
		setting.type = BT_SETTING_TYPE_DOUBLE;
		if (sizeof(double)==sizeof(l_txVerifyLEReturn.MAX_POWER_ACP_DBM[i]))    // Type_Checking
		{
			setting.value       = (void*)&l_txVerifyLEReturn.MAX_POWER_ACP_DBM[i];
			char tempStr[MAX_BUFFER_SIZE];
			sprintf_s(tempStr, "ACP_MAX_POWER_DBM_OFFSET_%d", i-5);
			setting.unit        = "dBm";
			setting.helpText    = "Reports max power in 1 MHz bands at specific offsets from center frequency. The offset in MHz is given in sequenceDefinition. Method according to 5.1.8 TRM/CA/06/C";
			l_txVerifyLEReturnMap.insert( pair<string,BT_SETTING_STRUCT>(tempStr, setting) );
		}
		else
		{
			printf("Parameter Type Error!\n");
			exit(1);
		}
	}

	l_txVerifyLEReturn.CABLE_LOSS_DB = NA_NUMBER;
	setting.type = BT_SETTING_TYPE_DOUBLE;
	if (sizeof(double)==sizeof(l_txVerifyLEReturn.CABLE_LOSS_DB))    // Type_Checking
	{
		setting.value       = (void*)&l_txVerifyLEReturn.CABLE_LOSS_DB;
		setting.unit        = "dB";
		setting.helpText    = "Cable loss from the DUT antenna port to tester";
		l_txVerifyLEReturnMap.insert( pair<string,BT_SETTING_STRUCT>("CABLE_LOSS_DB", setting) );
	}
	else
	{
		printf("Parameter Type Error!\n");
		exit(1);
	}

	// Error Message Return String
	l_txVerifyLEReturn.ERROR_MESSAGE[0] = '\0';
	setting.type = BT_SETTING_TYPE_STRING;
	if (MAX_BUFFER_SIZE==sizeof(l_txVerifyLEReturn.ERROR_MESSAGE))    // Type_Checking
	{
		setting.value       = (void*)l_txVerifyLEReturn.ERROR_MESSAGE;
		setting.unit        = "";
		setting.helpText    = "Error message occurred";
		l_txVerifyLEReturnMap.insert( pair<string,BT_SETTING_STRUCT>("ERROR_MESSAGE", setting) );
	}
	else
	{
		printf("Parameter Type Error!\n");
		exit(1);
	}

	return 0;
}

