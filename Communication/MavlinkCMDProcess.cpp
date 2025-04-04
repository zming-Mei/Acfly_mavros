#include "MavlinkCMDProcess.hpp"
#include "stm32h7xx_hal.h"
#include "mavlink.h"
#include "Commulink.hpp"
#include "Modes.hpp"
#include "Parameters.hpp"
#include "AC_Math.hpp"
#include "MeasurementSystem.hpp"
#include "AuxFuncs.hpp"
#include "ControlSystem.hpp"
#include "Sensors.hpp"
#include "StorageSystem.hpp"

//固件版本
static void Cmd520_MAV_CMD_REQUEST_AUTOPILOT_CAPABILITIES( uint8_t port_index , const mavlink_message_t* msg )
{
  const mavlink_command_long_t* msg_rd = (mavlink_command_long_t*)msg->payload64;
	const Port* port = get_CommuPort( port_index );
	if( port->write )
	{			
		__attribute__ ((aligned (4))) uint32_t flight_version = 0;
		
		//固件发行版本		
		((uint8_t*)&flight_version)[0] = FIRMWARE_VERSION_TYPE_OFFICIAL;
		
		//固件版本号
		float Firmware_V[2]={0};					
		ReadParam( "Init_Firmware_V", 0, 0, (uint64_t*)Firmware_V, 0 );
		uint8_t Firmware_Va = (int)(Firmware_V[0]*100) % 100;
		uint8_t Firmware_Vb = (int)(Firmware_V[0]*1) % 100;
		uint8_t Firmware_Vc = (int)(Firmware_V[0]*0.01f) % 100;
		((uint8_t*)&flight_version)[3] = Firmware_Vc;
		((uint8_t*)&flight_version)[2] = Firmware_Vb;
		((uint8_t*)&flight_version)[1] = Firmware_Va;
				
		mavlink_message_t msg_sd;	
		mavlink_msg_autopilot_version_pack_chan( 
				get_CommulinkSysId() ,	//system id
				get_CommulinkCompId() ,	//component id
				port_index ,
				&msg_sd,
				0,	//capabilities
				flight_version ,	//Firmware version number
				0 ,	//middleware_sw_version
				0 ,	//os_sw_version
				0 ,	//board_version
		    0 , //flight_custom_version
		    0 , //middleware_custom_version
		    0 , //os_custom_version
		    0 , //vendor_id
		    0 , //product_id
		    0 , //uid
		    0  	//uid2  
			);
			mavlink_msg_to_send_buffer(port->write, 
																 port->lock,
																 port->unlock,
																 &msg_sd, 0, 0.01);
			mavlink_unlock_chan(port_index);	
	}				
}
static void Cmd10_MAV_CMD_SEND_WGA( uint8_t port_index , const mavlink_message_t* msg )
{
	const mavlink_command_long_t* msg_rd = (mavlink_command_long_t*)msg->payload64;
	
	//发送正版验证标识符
	float buf;
	buf = msg_rd->param1;
	uint32_t ind = *(uint32_t*)&buf;
	
	uint32_t wga[3];
	MS_get_WGA(wga);
	bool wga_correct = MS_WGA_Correct();
	
	const Port* port = get_CommuPort( port_index );
	if( port->write )
	{
		mavlink_message_t msg_sd;
		if( mavlink_lock_chan( port_index, 0.01 ) )
		{
			mavlink_msg_command_ack_pack_chan_full(
				get_CommulinkSysId() ,	//system id
				get_CommulinkCompId() ,	//component id
				port_index ,	//chan
				&msg_sd,
				10 ,	//command
				wga_correct ,	//result
				ind ,	//progress
				wga[ind] ,	//result param2
				msg->sysid ,
				msg->compid
			);
			mavlink_msg_to_send_buffer(port->write, 
																 port->lock,
																 port->unlock,
																 &msg_sd, 0, 0.01);
			mavlink_unlock_chan(port_index);
		}
	}
}

static void Cmd11_MAV_CMD_WRITE_WGA( uint8_t port_index , const mavlink_message_t* msg )
{
	const mavlink_command_long_t* msg_rd = (mavlink_command_long_t*)msg->payload64;
	
	uint32_t wga[4];
	float buf;
	buf = msg_rd->param1;
	wga[0] = *(uint32_t*)&buf;
	buf = msg_rd->param2;
	wga[1] = *(uint32_t*)&buf;
	buf = msg_rd->param3;
	wga[2] = *(uint32_t*)&buf;
	buf = msg_rd->param4;
	wga[3] = *(uint32_t*)&buf;
	UpdateParamGroup( "WGA_Code", (uint64_t*)wga, 0, 2 );	
}

static void Cmd12_MAV_CMD_SET_RTC( uint8_t port_index , const mavlink_message_t* msg )
{
	const mavlink_command_long_t* msg_rd = (mavlink_command_long_t*)msg->payload64;
	
	RTC_TimeStruct rtc_time;
	
	//年月日时分秒
	float buf;
	buf = msg_rd->param1;
	uint32_t datetime = *(uint32_t*)&buf;
	rtc_time.Year = ( datetime >> 25 ) + 1980;
	rtc_time.Month = ( datetime >> 21 ) & 0xf;
	rtc_time.Date = ( datetime >> 16 ) & 0x1f;
	rtc_time.Hours = ( datetime >> 11 ) & 0x1f;
	rtc_time.Minutes = ( datetime >> 5 ) & 0x3f;
	rtc_time.Seconds = ( datetime >> 0 ) & 0x1f;
	
	//星期
	buf = msg_rd->param2;
	rtc_time.WeekDay = *(uint8_t*)&buf;
	
	Set_RTC_Time(&rtc_time);
}

static void Cmd511_MAV_CMD_SET_MESSAGE_INTERVAL( uint8_t port_index , const mavlink_message_t* msg )
{
	const mavlink_command_long_t* msg_rd = (mavlink_command_long_t*)msg->payload64;
}

static void Cmd519_MAV_CMD_REQUEST_PROTOCOL_VERSION( uint8_t port_index , const mavlink_message_t* msg )
{
	const mavlink_command_long_t* msg_rd = (mavlink_command_long_t*)msg->payload64;
	
	const Port* port = get_CommuPort( port_index );
	if( port->write )
	{
		mavlink_message_t msg_sd;
		uint8_t hash[8];
		if( mavlink_lock_chan( port_index, 0.01 ) )
		{
			mavlink_msg_protocol_version_pack_chan( 
				get_CommulinkSysId() ,	//system id
				get_CommulinkCompId() ,	//component id
				port_index ,
				&msg_sd,
				200,	//version
				100 ,	//min version
				200 ,	//max version
				hash ,	//
				hash	//
			);
			mavlink_msg_to_send_buffer(port->write, 
																 port->lock,
																 port->unlock,
																 &msg_sd, 0, 0.01);
			mavlink_unlock_chan(port_index);
		}
	}
}

static void Cmd410_MAV_CMD_GET_HOME_POSITION( uint8_t port_index , const mavlink_message_t* msg )
{
	double Altitude_Local=0;
	Position_Sensor GPS;
	vector2<double> homePLatLon,homePoint;
	if( getHomeLatLon(&homePLatLon) && getHomeLocalZ(&Altitude_Local) && getHomePoint(&homePoint))
	{
		const Port* port = get_CommuPort( port_index );
		if( port->write )
		{
			mavlink_message_t msg_sd;
			if( mavlink_lock_chan( port_index, 0.01 ) )
			{
				mavlink_msg_home_position_pack_chan(
					get_CommulinkSysId() ,	//system id
					get_CommulinkCompId() ,	//component id
					port_index ,	//chan
					&msg_sd,
					homePLatLon.x * 1e7 ,	//latitude
					homePLatLon.y * 1e7 ,	//longitude
					GPS.position.z * 10 ,	//altitude,mm
					homePoint.x * 0.01 , //Local X position
					homePoint.y * 0.01 , //Local Y position
				  Altitude_Local * 0.01 , //Local Z position
				  0 , //World to surface normal and heading transformation of the takeoff position
				  0 , //Local X position of the end of the approach vector
				  0 , //Local Y position of the end of the approach vector
				  0 , //Local Z position of the end of the approach vector
				  TIME::get_System_Run_Time()*1e6
				);
				mavlink_msg_to_send_buffer(port->write, 
																	 port->lock,
																	 port->unlock,
																	 &msg_sd, 0, 0.01);
				mavlink_unlock_chan(port_index);
			}
		}		
	}
}

/*拍照*/
	static void Cmd203_MAV_CMD_DO_DIGICAM_CONTROL( uint8_t port_index , const mavlink_message_t* msg )
	{
		const mavlink_command_long_t* msg_rd = (mavlink_command_long_t*)msg->payload64;
	
		bool res = AuxCamTakePhoto();
		if(res)
			sendLedSignal(LEDSignal_Success1);
		else
			sendLedSignal(LEDSignal_Err1);
		
		const Port* port = get_CommuPort( port_index );
		if( port->write )
		{
			mavlink_message_t msg_sd;
			if( mavlink_lock_chan( port_index, 0.01 ) )
			{
				mavlink_msg_command_ack_pack_chan( 
					get_CommulinkSysId() ,	//system id
					get_CommulinkCompId() ,	//component id
					port_index ,
					&msg_sd,
					msg_rd->command,	//command
					res ? MAV_RESULT_ACCEPTED : MAV_RESULT_DENIED ,	//result
					100 ,	//progress
					0 ,	//param2
					msg->sysid ,	//target system
					msg->compid //target component
				);
				mavlink_msg_to_send_buffer(port->write, 
																	 port->lock,
																	 port->unlock,
																	 &msg_sd, 0, 0.01);
				mavlink_unlock_chan(port_index);
			}
		}
	}
/*拍照*/

	
static void Cmd246_MAV_CMD_PREFLIGHT_REBOOT_SHUTDOWN( uint8_t port_index , const mavlink_message_t* msg )
{
	const mavlink_command_long_t* msg_rd = (mavlink_command_long_t*)msg->payload64;
	if(msg_rd->param1==1)
	{
		const Port* port = get_CommuPort( port_index );
		if( port->write )
		{
			mavlink_message_t msg_sd;
			if( mavlink_lock_chan( port_index, 0.01 ) )
			{
				mavlink_msg_command_ack_pack_chan( 
					get_CommulinkSysId() ,	//system id
					get_CommulinkCompId() ,	//component id
					port_index ,
					&msg_sd,
					msg_rd->command,	//command
					MAV_RESULT_ACCEPTED,	//result
					100 ,	//progress
					0 ,	//param2
					msg->sysid ,	//target system
					msg->compid //target component
				);
				mavlink_msg_to_send_buffer(port->write, 
																	 port->lock,
																	 port->unlock,
																	 &msg_sd, 0, 0.01);
				mavlink_unlock_chan(port_index);
			}
		}
		__set_FAULTMASK(1);
		HAL_NVIC_SystemReset();
	}
}

/******自定义接收消息*****/
double Debug_inform[3];
static void My_Debug_information( uint8_t port_index , const mavlink_message_t* msg )
{	

	const mavlink_command_long_t* msg_rd = (mavlink_command_long_t*)msg->payload64;
	Debug_inform[0] = msg_rd->param1;
	Debug_inform[1] = msg_rd->param2;
	Debug_inform[2] = msg_rd->param3;
	
//	if(abs(Debug_inform[0]-6),0.0001)
//		sendLedSignal(TWO_TIGER1);

//	SDLog_Msg_DebugVect( "My_infor", Debug_inform, 3 );		
	
//  float limit_yaw =msg_rd->param4; //yaw
//	float yaw_rate = msg_rd->param5;//yaw rate
//	const Port* port = get_Port( port_index );
//	if( port->write )
//	{
//		mavlink_message_t msg_sd;
//		mavlink_msg_command_ack_pack_chan( 
//			1 ,	//system id
//			MAV_COMP_ID_AUTOPILOT1 ,	//component id
//			port_index ,
//			&msg_sd,
//			msg_rd->command,	//command
//			 MAV_RESULT_ACCEPTED ,	//result ,	//result
//			100 ,	//progress
//			0 ,	//param2
//		msg->sysid ,
//		msg->compid
//		);
//		mavlink_msg_to_send_buffer(port->write, &msg_sd);
//	}	
}
/******自定义接收消息*****/

void (*const Mavlink_CMD_Process[])( uint8_t port_index , const mavlink_message_t* msg_rd ) = 
{
	/*000-*/	My_Debug_information,
	/*001-*/	0	,
	/*002-*/	0	,
	/*003-*/	0	,
	/*004-*/	0	,
	/*005-*/	0	,
	/*006-*/	0	,
	/*007-*/	0	,
	/*008-*/	0	,
	/*009-*/	0	,
	/*010-*/	Cmd10_MAV_CMD_SEND_WGA	,
	/*011-*/	Cmd11_MAV_CMD_WRITE_WGA	,
	/*012-*/	Cmd12_MAV_CMD_SET_RTC	,
	/*013-*/	0	,
	/*014-*/	0	,
	/*015-*/	0	,
	/*016-*/	0	,
	/*017-*/	0	,
	/*018-*/	0	,
	/*019-*/	0	,
	/*020-*/	0	,
	/*021-*/	0	,
	/*022-*/	0	,
	/*023-*/	0	,
	/*024-*/	0	,
	/*025-*/	0	,
	/*026-*/	0	,
	/*027-*/	0	,
	/*028-*/	0	,
	/*029-*/	0	,
	/*030-*/	0	,
	/*031-*/	0	,
	/*032-*/	0	,
	/*033-*/	0	,
	/*034-*/	0	,
	/*035-*/	0	,
	/*036-*/	0	,
	/*037-*/	0	,
	/*038-*/	0	,
	/*039-*/	0	,
	/*040-*/	0	,
	/*041-*/	0	,
	/*042-*/	0	,
	/*043-*/	0	,
	/*044-*/	0	,
	/*045-*/	0	,
	/*046-*/	0	,
	/*047-*/	0	,
	/*048-*/	0	,
	/*049-*/	0	,
	/*050-*/	0	,
	/*051-*/	0	,
	/*052-*/	0	,
	/*053-*/	0	,
	/*054-*/	0	,
	/*055-*/	0	,
	/*056-*/	0	,
	/*057-*/	0	,
	/*058-*/	0	,
	/*059-*/	0	,
	/*060-*/	0	,
	/*061-*/	0	,
	/*062-*/	0	,
	/*063-*/	0	,
	/*064-*/	0	,
	/*065-*/	0	,
	/*066-*/	0	,
	/*067-*/	0	,
	/*068-*/	0	,
	/*069-*/	0	,
	/*070-*/	0	,
	/*071-*/	0	,
	/*072-*/	0	,
	/*073-*/	0	,
	/*074-*/	0	,
	/*075-*/	0	,
	/*076-*/	0	,
	/*077-*/	0	,
	/*078-*/	0	,
	/*079-*/	0	,
	/*080-*/	0	,
	/*081-*/	0	,
	/*082-*/	0	,
	/*083-*/	0	,
	/*084-*/	0	,
	/*085-*/	0	,
	/*086-*/	0	,
	/*087-*/	0	,
	/*088-*/	0	,
	/*089-*/	0	,
	/*090-*/	0	,
	/*091-*/	0	,
	/*092-*/	0	,
	/*093-*/	0	,
	/*094-*/	0	,
	/*095-*/	0	,
	/*096-*/	0	,
	/*097-*/	0	,
	/*098-*/	0	,
	/*099-*/	0	,
	/*100-*/	0	,
	/*101-*/	0	,
	/*102-*/	0	,
	/*103-*/	0	,
	/*104-*/	0	,
	/*105-*/	0	,
	/*106-*/	0	,
	/*107-*/	0	,
	/*108-*/	0	,
	/*109-*/	0	,
	/*110-*/	0	,
	/*111-*/	0	,
	/*112-*/	0	,
	/*113-*/	0	,
	/*114-*/	0	,
	/*115-*/	0	,
	/*116-*/	0	,
	/*117-*/	0	,
	/*118-*/	0	,
	/*119-*/	0	,
	/*120-*/	0	,
	/*121-*/	0	,
	/*122-*/	0	,
	/*123-*/	0	,
	/*124-*/	0	,
	/*125-*/	0	,
	/*126-*/	0	,
	/*127-*/	0	,
	/*128-*/	0	,
	/*129-*/	0	,
	/*130-*/	0	,
	/*131-*/	0	,
	/*132-*/	0	,
	/*133-*/	0	,
	/*134-*/	0	,
	/*135-*/	0	,
	/*136-*/	0	,
	/*137-*/	0	,
	/*138-*/	0	,
	/*139-*/	0	,
	/*140-*/	0	,
	/*141-*/	0	,
	/*142-*/	0	,
	/*143-*/	0	,
	/*144-*/	0	,
	/*145-*/	0	,
	/*146-*/	0	,
	/*147-*/	0	,
	/*148-*/	0	,
	/*149-*/	0	,
	/*150-*/	0	,
	/*151-*/	0	,
	/*152-*/	0	,
	/*153-*/	0	,
	/*154-*/	0	,
	/*155-*/	0	,
	/*156-*/	0	,
	/*157-*/	0	,
	/*158-*/	0	,
	/*159-*/	0	,
	/*160-*/	0	,
	/*161-*/	0	,
	/*162-*/	0	,
	/*163-*/	0	,
	/*164-*/	0	,
	/*165-*/	0	,
	/*166-*/	0	,
	/*167-*/	0	,
	/*168-*/	0	,
	/*169-*/	0	,
	/*170-*/	0	,
	/*171-*/	0	,
	/*172-*/	0	,
	/*173-*/	0	,
	/*174-*/	0	,
	/*175-*/	0	,
	/*176-*/	0	,
	/*177-*/	0	,
	/*178-*/	0	,
	/*179-*/	0	,
	/*180-*/	0	,
	/*181-*/	0	,
	/*182-*/	0	,
	/*183-*/	0	,
	/*184-*/	0	,
	/*185-*/	0	,
	/*186-*/	0	,
	/*187-*/	0	,
	/*188-*/	0	,
	/*189-*/	0	,
	/*190-*/	0	,
	/*191-*/	0	,
	/*192-*/	0	,
	/*193-*/	0	,
	/*194-*/	0	,
	/*195-*/	0	,
	/*196-*/	0	,
	/*197-*/	0	,
	/*198-*/	0	,
	/*199-*/	0	,
	/*200-*/	0	,
	/*201-*/	0	,
	/*202-*/	0	,
	/*203-*/	Cmd203_MAV_CMD_DO_DIGICAM_CONTROL	,
	/*204-*/	0	,
	/*205-*/	0	,
	/*206-*/	0	,
	/*207-*/	0	,
	/*208-*/	0	,
	/*209-*/	0	,
	/*210-*/	0	,
	/*211-*/	0	,
	/*212-*/	0	,
	/*213-*/	0	,
	/*214-*/	0	,
	/*215-*/	0	,
	/*216-*/	0	,
	/*217-*/	0	,
	/*218-*/	0	,
	/*219-*/	0	,
	/*220-*/	0	,
	/*221-*/	0	,
	/*222-*/	0	,
	/*223-*/	0	,
	/*224-*/	0	,
	/*225-*/	0	,
	/*226-*/	0	,
	/*227-*/	0	,
	/*228-*/	0	,
	/*229-*/	0	,
	/*230-*/	0	,
	/*231-*/	0	,
	/*232-*/	0	,
	/*233-*/	0	,
	/*234-*/	0	,
	/*235-*/	0	,
	/*236-*/	0	,
	/*237-*/	0	,
	/*238-*/	0	,
	/*239-*/	0	,
	/*240-*/	0	,
	/*241-*/	0	,
	/*242-*/	0	,
	/*243-*/	0	,
	/*244-*/	0	,
	/*245-*/	0	,
	/*246-*/	Cmd246_MAV_CMD_PREFLIGHT_REBOOT_SHUTDOWN,
	/*247-*/	0	,
	/*248-*/	0	,
	/*249-*/	0	,
	/*250-*/	0	,
	/*251-*/	0	,
	/*252-*/	0	,
	/*253-*/	0	,
	/*254-*/	0	,
	/*255-*/	0	,
	/*256-*/	0	,
	/*257-*/	0	,
	/*258-*/	0	,
	/*259-*/	0	,
	/*260-*/	0	,
	/*261-*/	0	,
	/*262-*/	0	,
	/*263-*/	0	,
	/*264-*/	0	,
	/*265-*/	0	,
	/*266-*/	0	,
	/*267-*/	0	,
	/*268-*/	0	,
	/*269-*/	0	,
	/*270-*/	0	,
	/*271-*/	0	,
	/*272-*/	0	,
	/*273-*/	0	,
	/*274-*/	0	,
	/*275-*/	0	,
	/*276-*/	0	,
	/*277-*/	0	,
	/*278-*/	0	,
	/*279-*/	0	,
	/*280-*/	0	,
	/*281-*/	0	,
	/*282-*/	0	,
	/*283-*/	0	,
	/*284-*/	0	,
	/*285-*/	0	,
	/*286-*/	0	,
	/*287-*/	0	,
	/*288-*/	0	,
	/*289-*/	0	,
	/*290-*/	0	,
	/*291-*/	0	,
	/*292-*/	0	,
	/*293-*/	0	,
	/*294-*/	0	,
	/*295-*/	0	,
	/*296-*/	0	,
	/*297-*/	0	,
	/*298-*/	0	,
	/*299-*/	0	,
	/*300-*/	0	,
	/*301-*/	0	,
	/*302-*/	0	,
	/*303-*/	0	,
	/*304-*/	0	,
	/*305-*/	0	,
	/*306-*/	0	,
	/*307-*/	0	,
	/*308-*/	0	,
	/*309-*/	0	,
	/*310-*/	0	,
	/*311-*/	0	,
	/*312-*/	0	,
	/*313-*/	0	,
	/*314-*/	0	,
	/*315-*/	0	,
	/*316-*/	0	,
	/*317-*/	0	,
	/*318-*/	0	,
	/*319-*/	0	,
	/*320-*/	0	,
	/*321-*/	0	,
	/*322-*/	0	,
	/*323-*/	0	,
	/*324-*/	0	,
	/*325-*/	0	,
	/*326-*/	0	,
	/*327-*/	0	,
	/*328-*/	0	,
	/*329-*/	0	,
	/*330-*/	0	,
	/*331-*/	0	,
	/*332-*/	0	,
	/*333-*/	0	,
	/*334-*/	0	,
	/*335-*/	0	,
	/*336-*/	0	,
	/*337-*/	0	,
	/*338-*/	0	,
	/*339-*/	0	,
	/*340-*/	0	,
	/*341-*/	0	,
	/*342-*/	0	,
	/*343-*/	0	,
	/*344-*/	0	,
	/*345-*/	0	,
	/*346-*/	0	,
	/*347-*/	0	,
	/*348-*/	0	,
	/*349-*/	0	,
	/*350-*/	0	,
	/*351-*/	0	,
	/*352-*/	0	,
	/*353-*/	0	,
	/*354-*/	0	,
	/*355-*/	0	,
	/*356-*/	0	,
	/*357-*/	0	,
	/*358-*/	0	,
	/*359-*/	0	,
	/*360-*/	0	,
	/*361-*/	0	,
	/*362-*/	0	,
	/*363-*/	0	,
	/*364-*/	0	,
	/*365-*/	0	,
	/*366-*/	0	,
	/*367-*/	0	,
	/*368-*/	0	,
	/*369-*/	0	,
	/*370-*/	0	,
	/*371-*/	0	,
	/*372-*/	0	,
	/*373-*/	0	,
	/*374-*/	0	,
	/*375-*/	0	,
	/*376-*/	0	,
	/*377-*/	0	,
	/*378-*/	0	,
	/*379-*/	0	,
	/*380-*/	0	,
	/*381-*/	0	,
	/*382-*/	0	,
	/*383-*/	0	,
	/*384-*/	0	,
	/*385-*/	0	,
	/*386-*/	0	,
	/*387-*/	0	,
	/*388-*/	0	,
	/*389-*/	0	,
	/*390-*/	0	,
	/*391-*/	0	,
	/*392-*/	0	,
	/*393-*/	0	,
	/*394-*/	0	,
	/*395-*/	0	,
	/*396-*/	0	,
	/*397-*/	0	,
	/*398-*/	0	,
	/*399-*/	0	,
	/*400-*/	0	,
	/*401-*/	0	,
	/*402-*/	0	,
	/*403-*/	0	,
	/*404-*/	0	,
	/*405-*/	0	,
	/*406-*/	0	,
	/*407-*/	0	,
	/*408-*/	0	,
	/*409-*/	0	,
	/*410-*/	Cmd410_MAV_CMD_GET_HOME_POSITION	,
	/*411-*/	0	,
	/*412-*/	0	,
	/*413-*/	0	,
	/*414-*/	0	,
	/*415-*/	0	,
	/*416-*/	0	,
	/*417-*/	0	,
	/*418-*/	0	,
	/*419-*/	0	,
	/*420-*/	0	,
	/*421-*/	0	,
	/*422-*/	0	,
	/*423-*/	0	,
	/*424-*/	0	,
	/*425-*/	0	,
	/*426-*/	0	,
	/*427-*/	0	,
	/*428-*/	0	,
	/*429-*/	0	,
	/*430-*/	0	,
	/*431-*/	0	,
	/*432-*/	0	,
	/*433-*/	0	,
	/*434-*/	0	,
	/*435-*/	0	,
	/*436-*/	0	,
	/*437-*/	0	,
	/*438-*/	0	,
	/*439-*/	0	,
	/*440-*/	0	,
	/*441-*/	0	,
	/*442-*/	0	,
	/*443-*/	0	,
	/*444-*/	0	,
	/*445-*/	0	,
	/*446-*/	0	,
	/*447-*/	0	,
	/*448-*/	0	,
	/*449-*/	0	,
	/*450-*/	0	,
	/*451-*/	0	,
	/*452-*/	0	,
	/*453-*/	0	,
	/*454-*/	0	,
	/*455-*/	0	,
	/*456-*/	0	,
	/*457-*/	0	,
	/*458-*/	0	,
	/*459-*/	0	,
	/*460-*/	0	,
	/*461-*/	0	,
	/*462-*/	0	,
	/*463-*/	0	,
	/*464-*/	0	,
	/*465-*/	0	,
	/*466-*/	0	,
	/*467-*/	0	,
	/*468-*/	0	,
	/*469-*/	0	,
	/*470-*/	0	,
	/*471-*/	0	,
	/*472-*/	0	,
	/*473-*/	0	,
	/*474-*/	0	,
	/*475-*/	0	,
	/*476-*/	0	,
	/*477-*/	0	,
	/*478-*/	0	,
	/*479-*/	0	,
	/*480-*/	0	,
	/*481-*/	0	,
	/*482-*/	0	,
	/*483-*/	0	,
	/*484-*/	0	,
	/*485-*/	0	,
	/*486-*/	0	,
	/*487-*/	0	,
	/*488-*/	0	,
	/*489-*/	0	,
	/*490-*/	0	,
	/*491-*/	0	,
	/*492-*/	0	,
	/*493-*/	0	,
	/*494-*/	0	,
	/*495-*/	0	,
	/*496-*/	0	,
	/*497-*/	0	,
	/*498-*/	0	,
	/*499-*/	0	,
	/*500-*/	0	,
	/*501-*/	0	,
	/*502-*/	0	,
	/*503-*/	0	,
	/*504-*/	0	,
	/*505-*/	0	,
	/*506-*/	0	,
	/*507-*/	0	,
	/*508-*/	0	,
	/*509-*/	0	,
	/*510-*/	0	,
	/*511-*/	Cmd511_MAV_CMD_SET_MESSAGE_INTERVAL	,
	/*512-*/	0	,
	/*513-*/	0	,
	/*514-*/	0	,
	/*515-*/	0	,
	/*516-*/	0	,
	/*517-*/	0	,
	/*518-*/	0	,
	/*519-*/	Cmd519_MAV_CMD_REQUEST_PROTOCOL_VERSION	,
	/*520-*/	Cmd520_MAV_CMD_REQUEST_AUTOPILOT_CAPABILITIES	,
	/*521-*/	0	,
	/*522-*/	0	,
	/*523-*/	0	,
	/*524-*/	0	,
	/*525-*/	0	,
	/*526-*/	0	,
	/*527-*/	0	,
	/*528-*/	0	,
	/*529-*/	0	,
	/*530-*/	0	,
	/*531-*/	0	,
	/*532-*/	0	,
	/*533-*/	0	,
	/*534-*/	0	,
	/*535-*/	0	,
	/*536-*/	0	,
	/*537-*/	0	,
	/*538-*/	0	,
	/*539-*/	0	,
	/*540-*/	0	,
	/*541-*/	0	,
	/*542-*/	0	,
	/*543-*/	0	,
	/*544-*/	0	,
	/*545-*/	0	,
	/*546-*/	0	,
	/*547-*/	0	,
	/*548-*/	0	,
	/*549-*/	0	,
	/*550-*/	0	,
	/*551-*/	0	,
	/*552-*/	0	,
	/*553-*/	0	,
	/*554-*/	0	,
	/*555-*/	0	,
	/*556-*/	0	,
	/*557-*/	0	,
	/*558-*/	0	,
	/*559-*/	0	,
	/*560-*/	0	,
	/*561-*/	0	,
	/*562-*/	0	,
	/*563-*/	0	,
	/*564-*/	0	,
	/*565-*/	0	,
	/*566-*/	0	,
	/*567-*/	0	,
	/*568-*/	0	,
	/*569-*/	0	,
	/*570-*/	0	,
	/*571-*/	0	,
	/*572-*/	0	,
	/*573-*/	0	,
	/*574-*/	0	,
	/*575-*/	0	,
	/*576-*/	0	,
	/*577-*/	0	,
	/*578-*/	0	,
	/*579-*/	0	,
	/*580-*/	0	,
	/*581-*/	0	,
	/*582-*/	0	,
	/*583-*/	0	,
	/*584-*/	0	,
	/*585-*/	0	,
	/*586-*/	0	,
	/*587-*/	0	,
	/*588-*/	0	,
	/*589-*/	0	,
	/*590-*/	0	,
	/*591-*/	0	,
	/*592-*/	0	,
	/*593-*/	0	,
	/*594-*/	0	,
	/*595-*/	0	,
	/*596-*/	0	,
	/*597-*/	0	,
	/*598-*/	0	,
	/*599-*/	0	,
};
const uint16_t Mavlink_CMD_Process_Count = sizeof( Mavlink_CMD_Process ) / sizeof( void* );