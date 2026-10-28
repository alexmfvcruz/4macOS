#pragma once
typedef enum MANAGER_CLASS
{
	MANAGER_ALL = 1,	// 모두 사용
	MANAGER_CONTROL ,	// 패치/업로드
	MANAGER_USER ,		// 공지사항/강제퇴장/위치이동
	MANAGER_SERVICE ,	// 서비스 ON/OFF	
	MANAGER_GMLEVEL1 ,	// GMTool 의 모든 기능
	MANAGER_GMLEVEL2 ,  // GMTool 의 하위기능
	MANAGER_GMLEVEL3 ,
	MANAGER_COUNT

} *LPMANAGER_CLASS;
