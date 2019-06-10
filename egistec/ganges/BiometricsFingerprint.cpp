#include "BiometricsFingerprint.h"
#include "FormatException.hpp"

#define LOG_TAG "FPC ET"
#include <log/log.h>

namespace egistec::ganges {

BiometricsFingerprint::BiometricsFingerprint(EgisFpDevice &&dev) : mDev(std::move(dev)) {
    QSEEKeymasterTrustlet keymaster;
    int rc = 0;

    DeviceEnableGuard<EgisFpDevice> guard{mDev};
    mDev.Enable();

    mMasterKey = keymaster.GetKey();

    rc = mTrustlet.SetDataPath("/data/system/users/0/fpdata");
    LOG_ALWAYS_FATAL_IF(rc, "SetDataPath failed with rc = %d", rc);

    rc = mTrustlet.SetMasterKey(mMasterKey);
    LOG_ALWAYS_FATAL_IF(rc, "SetMasterKey failed with rc = %d", rc);

    rc = mTrustlet.InitializeSensor();
    LOG_ALWAYS_FATAL_IF(rc, "InitializeSensor failed with rc = %d", rc);

    rc = mTrustlet.InitializeAlgo();
    LOG_ALWAYS_FATAL_IF(rc, "InitializeAlgo failed with rc = %d", rc);

    rc = mTrustlet.Calibrate();
    LOG_ALWAYS_FATAL_IF(rc, "Calibrate failed with rc = %d", rc);

    // TODO: From thread
    // Power saving?
    rc = mTrustlet.SetWorkMode(2);
    if (rc)
        throw FormatException("SetWorkMode failed with rc = %d", rc);
}

Return<uint64_t> BiometricsFingerprint::setNotify(const sp<IBiometricsFingerprintClientCallback> &clientCallback) {
    std::lock_guard<std::mutex> lock(mClientCallbackMutex);
    mClientCallback = clientCallback;
    // This is here because HAL 2.1 doesn't have a way to propagate a
    // unique token for its driver. Subsequent versions should send a unique
    // token for each call to setNotify(). This is fine as long as there's only
    // one fingerprint device on the platform.
    return reinterpret_cast<uint64_t>(this);
}

Return<uint64_t> BiometricsFingerprint::preEnroll() {
    // TODO:
    auto challenge = -1ul;
    ALOGI("%s: Generated enroll challenge %#lx", __func__, challenge);
    return challenge;
}

Return<RequestStatus> BiometricsFingerprint::enroll(const hidl_array<uint8_t, 69> &hat, uint32_t gid, uint32_t /* timeoutSec */) {
    if (gid != mGid) {
        ALOGE("Cannot enroll finger for different gid! Caller needs to update storePath first with setActiveGroup()!");
        return RequestStatus::SYS_EINVAL;
    }

    const auto h = reinterpret_cast<const hw_auth_token_t *>(hat.data());
    if (!h) {
        // This seems to happen when locking the device while enrolling.
        // It is unknown why this function is called again.
        ALOGE("%s: authentication token is unset!", __func__);
        return RequestStatus::SYS_EINVAL;
    }

    ALOGI("Starting enroll for challenge %#lx", h->challenge);
    // TODO:
    return RequestStatus::SYS_EFAULT;
    // return loops.Enroll(*h, timeoutSec) ? RequestStatus::SYS_EINVAL : RequestStatus::SYS_OK;
}

Return<RequestStatus> BiometricsFingerprint::postEnroll() {
    ALOGI("%s: clearing challenge", __func__);
    // TODO:
    return RequestStatus::SYS_EFAULT;
    // return loops.ClearChallenge() ? RequestStatus::SYS_UNKNOWN : RequestStatus::SYS_OK;
}

Return<uint64_t> BiometricsFingerprint::getAuthenticatorId() {
    auto id = mTrustlet.GetAuthenticatorId();
    ALOGI("%s: id = %lu", __func__, id);
    return id;
}

Return<RequestStatus> BiometricsFingerprint::cancel() {
    ALOGI("Cancel requested");
    return RequestStatus::SYS_EFAULT;
    // bool success = loops.Cancel();
    // return success ? RequestStatus::SYS_OK : RequestStatus::SYS_UNKNOWN;
}

Return<RequestStatus> BiometricsFingerprint::enumerate() {
    std::vector<uint32_t> fids;
    int rc = mTrustlet.GetPrintIds(mGid, fids);
    if (rc)
        return RequestStatus::SYS_EINVAL;

    auto remaining = fids.size();
    ALOGD("Enumerating %zu fingers", remaining);

    std::lock_guard<std::mutex> lock(mClientCallbackMutex);
    if (!remaining)
        // If no fingerprints exist, notify that the enumeration is done with remaining=0.
        // Use fid=0 to indicate that this is not a fingerprint.
        mClientCallback->onEnumerate(reinterpret_cast<uint64_t>(this), 0, mGid, 0);
    else
        for (auto fid : fids)
            mClientCallback->onEnumerate(reinterpret_cast<uint64_t>(this), fid, mGid, --remaining);

    return RequestStatus::SYS_OK;
}

Return<RequestStatus> BiometricsFingerprint::remove(uint32_t gid, uint32_t fid) {
    ALOGI("%s: gid = %d, fid = %d", __func__, gid, fid);
    if (gid != mGid) {
        ALOGE("Change group and userpath through setActiveGroup first!");
        return RequestStatus::SYS_EINVAL;
    }
    return RequestStatus::SYS_EFAULT;
    // return loops.RemoveFinger(fid) ? RequestStatus::SYS_EINVAL : RequestStatus::SYS_OK;
}

Return<RequestStatus> BiometricsFingerprint::setActiveGroup(uint32_t gid, const hidl_string &storePath) {
    ALOGI("%s: gid = %u, path = %s", __func__, gid, storePath.c_str());
    mGid = gid;
    int rc = mTrustlet.SetUserDataPath(gid, storePath.c_str());
    return rc ? RequestStatus::SYS_EINVAL : RequestStatus::SYS_OK;
}

Return<RequestStatus> BiometricsFingerprint::authenticate(uint64_t operationId, uint32_t gid) {
    ALOGI("%s: gid = %d, secret = %lu", __func__, gid, operationId);
    if (gid != mGid) {
        ALOGE("Cannot authenticate finger for different gid! Caller needs to update storePath first with setActiveGroup()!");
        return RequestStatus::SYS_EINVAL;
    }

    return RequestStatus::SYS_EFAULT;
    // return loops.Authenticate(operationId) ? RequestStatus::SYS_EINVAL : RequestStatus::SYS_OK;
}

}  // namespace egistec::ganges
