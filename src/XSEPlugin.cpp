
#include <ENB/ENBSeriesAPI.h>
#include "Reflex.h"

ENB_API::ENBSDKALT1001* g_ENB = nullptr;

static void MessageHandler(SKSE::MessagingInterface::Message* a_msg)
{
	switch (a_msg->type) {
	case SKSE::MessagingInterface::kPostLoad:
		g_ENB = reinterpret_cast<ENB_API::ENBSDKALT1001*>(ENB_API::RequestENBAPI(ENB_API::SDKVersion::V1001));
		if (g_ENB) {
			logger::info("Obtained ENB API");
			g_ENB->SetCallbackFunction([](ENBCallbackType calltype) {
				switch (calltype) {
				case ENBCallbackType::ENBCallback_PostLoad:
					Reflex::GetSingleton()->LoadINI();
					Reflex::GetSingleton()->RefreshUI();
					break;
				case ENBCallbackType::ENBCallback_PreSave:
					Reflex::GetSingleton()->SaveINI();
					Reflex::GetSingleton()->RefreshUI();
					break;
				case ENBCallbackType::ENBCallback_OnInit:;
					Reflex::GetSingleton()->RefreshUI();
					break;
				case ENBCallbackType::ENBCallback_PostReset:;
					Reflex::GetSingleton()->RefreshUI();
					break;
				}
			});
		} else
			logger::info("Unable to acquire ENB API");

		break;
	}
}

void PatchD3D11();

void Load()
{
	SKSE::GetMessagingInterface()->RegisterListener(MessageHandler);
	PatchD3D11();
	Reflex::InstallHooks();
}