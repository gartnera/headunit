#pragma once

#include <string>
#include <future>
#include "../dbus/generated_cmu.h"
#include "hu.pb.h"


class BDSClient : public com::jci::bds_proxy, public DBus::ObjectProxy
{
public:
    BDSClient(DBus::Connection &connection, const char *path, const char *name): DBus::ObjectProxy(connection, path, name){}

    void Signaltest_cb(const uint32_t& type) override {}
    void SignalReady_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalNotReady_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalDeviceDeleteInProgress_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalDeviceDeleted_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalDeviceDeleteAllInProgress_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalDeviceDeleteAll_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalDeviceDisconnectedAll_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalDeviceDisconnectionProgress_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalDeviceInquiryResult_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalDeviceNameStarted_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalDeviceNameSet_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalDeviceNameGet_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalDeviceAccessibilityGet_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalDevicePair_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalDeviceServiceDiscovery_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalDeviceWrittenAll_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalDeviceAuthenticate_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalRemoteSupportedFeatures_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalDeviceGetConnectedDevices_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalDeviceLocalOobData_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalDeviceExtendedSearchResult_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalServiceEnableStatus_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalServiceDisableStatus_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalConnected_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override;
    void SignalDisconnected_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalServiceFeatures_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalServiceConfiguration_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalAudioSinkAppChange_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalAudioSinkBatteryStatusChange_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalAudioSinkBiased_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalAudioSinkBrowseConnect_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalAudioSinkBrowseDisonnect_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalAudioSinkBrowseFolderChange_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalAudioSinkBrowseFolderContent_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalAudioSinkBrowseFolderContents_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalAudioSinkBrowseSearch_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalAudioSinkBrowseUidsChange_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalAudioSinkContentAddToNowPlaying_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalAudioSinkContentAttributes_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalAudioSinkContentPlay_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalAudioSinkMediaPosition_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalAudioSinkPanelOperation_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalAudioSinkPlaybackStatusChange_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalAudioSinkPlayerAddressedChange_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalAudioSinkPlayerBrowsedChange_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalAudioSinkPlayerItem_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalAudioSinkPlayerItems_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalAudioSinkPlayerItemsChange_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalAudioSinkPlayerNowContentChange_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalAudioSinkPlayerVolumeChange_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalAudioSinkStreamClosed_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalAudioSinkStreamConfigured_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalAudioSinkStreamLine_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalAudioSinkStreamMuted_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalAudioSinkStreamOpened_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalAudioSinkSupportedChanges_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalAudioSinkSupportedPlayerSetting_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalAudioSinkSystemStatusChange_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalAudioSinkTrackChange_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalAudioSinkTrackEnd_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalAudioSinkTrackMetaData_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalAudioSinkTrackStart_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalCallClientCallList_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalCallClientStatus_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalCallClientAudioConnect_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalCallClientAudioDisconnect_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalCallClientIndicator_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalCallClientOperator_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalCallClientSubscriber_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalCallClientMicVolumeChange_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalCallClientSpeakerVolumeChange_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalCallClientActivateVoiceDial_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalCallClientDeActivateVoiceDial_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalCallClientSiriSupportStatus_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalCallClientSiriEyesFreeMode_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalCallClientSiriDisableNoiseReduction_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalPhoneStatus_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalCallClientCmdError_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalCallClientScoCodecId_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalMessageClientConfigured_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalMessageClientFolderListing_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalMessageClientMessageDeleted_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalMessageClientMessageGotten_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalMessageClientMessageListing_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalMessageClientMessageNotification_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalMessageClientMessagePushed_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalMessageClientMessageSetStatus_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalNetworkClientConnected_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalNetworkClientDisconnected_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalPimClientImportStatus_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalFileServerInfoEvent_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalFileServerFileAcceptanceStatus_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalFileServerDataFileInProgressStatus_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalServiceLessAudioConnectStatus_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalServiceLessAudioDisconnectStatus_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalServiceLessAclConnectStatus_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalServiceLessAclDisconnectStatus_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalBTChipFailure_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}
    void SignalDeviceAddressGet_cb(const uint32_t& type, const ::DBus::Struct< std::vector< uint8_t > >& data) override {}

};

class BCAClient : public com::jci::bca_proxy, public DBus::ObjectProxy {
public:
    BCAClient(DBus::Connection &connection, const char *path, const char *name) : DBus::ObjectProxy(connection, path, name) {}
    void DontShowOnConnectionSettingStatusResp(const uint32_t& btDeviceId, const bool& setting) override {}
    void ConnectionStatusResp(const uint32_t& serviceId, const uint32_t& connStatus, const uint32_t& btDeviceId, const uint32_t& status, const ::DBus::Struct< std::vector< uint8_t > >& terminalPath) override;
    void HftReadyStatus(const uint32_t& hftReady, const uint32_t& reasonCode, const uint32_t& appId) override {}
    void ReadyStatus(const uint32_t& isReady, const uint32_t& reasonCode) override {}
    void AddStatusResp(const uint32_t& addServiceId, const uint32_t& status) override {}
    void DeviceUnbarringStatusResp(const uint32_t& btDeviceId, const uint32_t& status) override {}
    void DeviceBluetoothSettingResp(const bool& btSetting) override {}
    void AvailablePairedListResp(const uint32_t& totalPairedDevices, const ::DBus::Struct< std::vector< uint8_t > >& pairedDeviceList) override {}
    void SupportedProfilesResp(const ::DBus::Struct< std::vector< uint8_t > >& supportedProfiles) override {}
    void FirstHfpSupportedInfo(const bool& isHfpSupportedDevicePresent) override {}
    void CarPlayConnectionStatus(const uint32_t& connStatus, const uint32_t& carPlayDeviceId) override {}
    void AAutoEnableBt() override {}
    void AAutoNoEntryAvailable(const uint32_t& deleteDeviceId) override {}
    void ReqStartAndroidAutoPairing() override {}
    void ReqStopAndroidAutoPairing() override {}
    void ReqDeleteDevice(const uint32_t& deleteDeviceId) override {}
    void AAPairingSeqResult(const bool& result) override {}
    void AndroidAutoPairingTimeout() override {}
    void RequestStartAutoDownload(const uint32_t& btDeviceId) override {}
    void DisableBluetoothRsp(const uint32_t& activeCallStatus) override {}
    void ConnectingCarPlayError() override {}
};

void sendMessage(int fd, google::protobuf::MessageLite &message, uint16_t type);
void handleWifiInfoRequest(int fd, uint8_t *buffer, uint16_t length);
void handleWifiSecurityRequest(int fd, uint8_t *buffer, uint16_t length);
int handleWifiInfoRequestResponse(int fd, uint8_t *buffer, uint16_t length);
std::string hostapd_config(const std::string& key);
void update_ip_mac();
void wireless_stop();
void wireless_thread(std::promise<int> * promObj, DBus::Connection *serviceBus, DBus::Connection *hmiBus);

