// Offline-only transport boundary for the entitlement regression executable.
// All exercised requests use upstream's injected FamilyPerform/AcquisitionSource.
// Abort if a test accidentally attempts a real HTTP request.
#include "steam/backend/cm_client.h"
#include <cstdlib>
namespace playden::steam {
class HttpClient::Impl {};
HttpClient::HttpClient(std::string) { std::abort(); }
HttpClient::~HttpClient() = default;
HttpResponse HttpClient::Perform(const HttpRequest&,const std::atomic<bool>&) { std::abort(); }
std::string HttpFailureName(const HttpResponse& response) {
  return response.error == HttpError::kTimeout ? "timeout" : "injected_transport_failure";
}
const char* CmErrorName(CmError e) {
  switch(e) {
    case CmError::kNone:return "none"; case CmError::kCanceled:return "canceled";
    case CmError::kNoSession:return "no_session"; case CmError::kNetwork:return "network";
    case CmError::kRejected:return "rejected"; case CmError::kProtocol:return "protocol";
    case CmError::kTimeout:return "timeout";
  }
  return "protocol";
}
}
