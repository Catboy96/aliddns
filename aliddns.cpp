/**********************************************************************************************************************
    file:           aliddns.c
    description:    aliddns main executable
    author:         (C) 2022 PlayerCatboy (Ralf Ren).
    date:           Jan.27, 2022
**********************************************************************************************************************/
#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>

#include <unistd.h>
#include <pwd.h>

#include <alibabacloud/core/AlibabaCloud.h>
#include <alibabacloud/core/CommonRequest.h>
#include <alibabacloud/core/CommonClient.h>
#include <jsoncpp/json/json.h>
#include <curl/curl.h>
#include <curl/easy.h>
/**********************************************************************************************************************
    common definitions
**********************************************************************************************************************/
#define WHITESPACE              " \n\r\t\f\v"

#define SDK_REGION_ID           "cn-qingdao"
#define SDK_REQUEST_DOMAIN      "alidns.cn-hangzhou.aliyuncs.com"
#define SDK_REQUEST_VERSION     "2015-01-09"

#define DEFAULT_CONN_TIMEOUT    (1500)
#define DEFAULT_READ_TIMEOUT    (4000)
/**********************************************************************************************************************
    using
**********************************************************************************************************************/
using namespace std;
using namespace AlibabaCloud;
using namespace Json;
/**********************************************************************************************************************
    global variables
**********************************************************************************************************************/
static CommonClient *client = nullptr;
/**********************************************************************************************************************
    description:    trim left
    arguments:      s:  string
    return:         trimmed string
**********************************************************************************************************************/
static string gtb_ltrim (const string &s)
{
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}
/**********************************************************************************************************************
    description:    trim right
    arguments:      s:  string
    return:         trimmed string
**********************************************************************************************************************/
static string gtb_rtrim (const string &s)
{
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}
/**********************************************************************************************************************
    description:    trim all
    arguments:      s:  string
    return:         trimmed string
**********************************************************************************************************************/
static string gtb_trim (const string &s) {
    return gtb_rtrim(gtb_ltrim(s));
}
/**********************************************************************************************************************
    description:    alibaba cloud sdk init
    arguments:      id:         access key id
                    secret:     access key secret
                    conn_to:    connection timeout
                    read_to:    read timeout
    return:         error code
**********************************************************************************************************************/
static int gtb_sdk_init (const string &id, const string &secret, int conn_to, int read_to)
{
    // initialize alibaba cloud sdk
    InitializeSdk();
    if (!IsSdkInitialized()) {
        cerr << "Cannot parse configuration file." << endl;
        return -EBUSY;
    }

    // client configuration
    ClientConfiguration configuration(SDK_REGION_ID);
    configuration.setConnectTimeout(conn_to);
    configuration.setReadTimeout(read_to);

    // client authentication credentials
    AlibabaCloud::Credentials credential(id, secret);

    // create the client
    client = new CommonClient(credential, configuration);

    return 0;
}
/**********************************************************************************************************************
    description:    alibaba cloud sdk shutdown
    arguments:      -
    return:         -
**********************************************************************************************************************/
static inline void gtb_sdk_shutdown ()
{
    ShutdownSdk();
}
/**********************************************************************************************************************
    description:    get domain record
    arguments:      domain:     domain name
                    record_id:  alidns record id
                    value:      alidns record value
                    rr:         alidns record rr
    return:         error code
**********************************************************************************************************************/
static int gtb_get_domain_record (const string  &domain,
                                  string        *record_id,
                                  string        *value,
                                  string        *rr)
{
    Reader      reader;
    Value       root;
    string      domain_record;

    if (domain.empty()) return -EINVAL;
    if (!client) return -EINVAL;

    // build sdk request
    CommonRequest request(AlibabaCloud::CommonRequest::RequestPattern::RpcPattern);
    request.setHttpMethod(AlibabaCloud::HttpRequest::Method::Post);
    request.setDomain(SDK_REQUEST_DOMAIN);
    request.setVersion(SDK_REQUEST_VERSION);
    request.setQueryParameter("Action", "DescribeSubDomainRecords");
    request.setQueryParameter("SubDomain", domain);

    // get response
    auto response = client->commonResponse(request);
    if (!response.isSuccess()) {
        cerr << "Get request failed: " << response.error().errorMessage() << endl;
        return -EBUSY;
    }

    // parse json
    if (!reader.parse(response.result().payload(), root)) {
        cout << "Invalid response: invalid JSON format." << endl;
        return -EINVAL;
    }

    // check if specified domain record exists
    if (!root["DomainRecords"]["Record"].isArray()
        || root["DomainRecords"]["Record"].empty()) {
        cerr << "Invalid response: empty domain record." << endl;
        return -EINVAL;
    }

    // check each record is match specified domain
    for (auto &iter : root["DomainRecords"]["Record"]) {
        domain_record = "";
        if (iter["RR"].asString() != "@") {
            domain_record += iter["RR"].asString() + ".";
        }
        domain_record += iter["DomainName"].asString();

        if (domain_record == domain) {
            *record_id  = iter["RecordId"].asString();
            *value      = iter["Value"].asString();
            *rr         = iter["RR"].asString();

            return 0;
        }
    }

    // no matches
    return -EINVAL;
}
/**********************************************************************************************************************
    description:    set domain record
    arguments:      record_id:  alidns record id
                    rr:         alidns record rr
                    value:      alidns record value
    return:         error code
**********************************************************************************************************************/
static int gtb_set_domain_record (const string &record_id,
                                  const string &rr,
                                  const string &value)
{
    if (record_id.empty()) return -EINVAL;
    if (value.empty()) return -EINVAL;
    if (rr.empty()) return -EINVAL;
    if (!client) return -EINVAL;

    // build request
    CommonRequest request(AlibabaCloud::CommonRequest::RequestPattern::RpcPattern);
    request.setHttpMethod(AlibabaCloud::HttpRequest::Method::Post);
    request.setDomain(SDK_REQUEST_DOMAIN);
    request.setVersion(SDK_REQUEST_VERSION);
    request.setQueryParameter("Action",     "UpdateDomainRecord");
    request.setQueryParameter("Type",       "A");
    request.setQueryParameter("RecordId",   record_id);
    request.setQueryParameter("RR",         rr);
    request.setQueryParameter("Value",      value);

    // get response
    auto response = client->commonResponse(request);
    if (!response.isSuccess()) {
        cerr << "Set request failed: " << response.error().errorMessage() << endl;
        return -EINVAL;
    }

    return 0;
}
/**********************************************************************************************************************
    description:    curl writer callback
    arguments:      data:       pointer to receive data
                    size:       always 1
                    nmemb:      data size
                    writerData: pointer to receive storage
    return:         size written
**********************************************************************************************************************/
static size_t gtb_curl_writer (char   *data,
                               size_t  size,
                               size_t  nmemb,
                               string *writerData)
{
    if (writerData == nullptr) return 0;

    writerData->append(data, size * nmemb);

    return size * nmemb;
}
/**********************************************************************************************************************
    description:    get current public ipv4 address
    arguments:      url:    ipv4 query url
                    ipv4:   ipv4 result
    return:         error code
**********************************************************************************************************************/
static int gtb_get_curr_ipv4 (const string  &url,
                              string        *ipv4)
{
    CURL        *curl;
    CURLcode     res;
    string       buffer;

    if (!ipv4) return -EINVAL;
    if (url.empty()) return -EINVAL;

    curl = curl_easy_init();
    if (!curl) {
        cerr << "CURL init failed." << endl;
        return -EBUSY;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, true);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, gtb_curl_writer);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);

    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        cerr << "CURL request failed." << endl;
        return -EBUSY;
    }

    curl_easy_cleanup(curl);

    *ipv4 = gtb_trim(buffer);

    return 0;
}
/**********************************************************************************************************************
    description:    application entry
    arguments:      argc:   command line arguments count
                    argv:   command line arguments string array
    return:         error code
**********************************************************************************************************************/
int main (int argc, char *argv[])
{
    Reader      reader;
    Value       root;
    ifstream    ifs;
    char       *home;
    int         conn_to, read_to;

    // get home directory path
    if ((home = getenv("HOME")) == nullptr) {
        home = getpwuid(getuid())->pw_dir;
    }

    // open configuration file
    ifs.open(string(home) + "/.ddns_config", ios::in);
    if (!ifs) {
        cerr << "Cannot open config file: " << string(home) + "/.ddns_config" << endl;
        return -1;
    }

    // parse configuration file
    if (!reader.parse(ifs, root)) {
        cerr << "Cannot parse configuration file." << endl;
        return -1;
    }

    // check if mandatory configuration exists
    if (!root["access_key_id"]) {
        cerr << "\"access_key_id\" must be specified in configuration file." << endl;
        return -EINVAL;
    }

    if (!root["access_key_secret"]) {
        cerr << "\"access_key_secret\" must be specified in configuration file." << endl;
        return -EINVAL;
    }

    if (!root["domain_name"]) {
        cerr << "\"domain_name\" must be specified in configuration file." << endl;
        return -EINVAL;
    }

    if (!root["query_ipv4"]) {
        cerr << "\"query_ipv4\" must be specified in configuration file." << endl;
        return -EINVAL;
    }

    // check if optional configuration exists
    if (!root["connect_timeout"]) {
        conn_to = DEFAULT_CONN_TIMEOUT;
    } else {
        conn_to = root["connect_timeout"].asInt();
    }

    if (!root["read_timeout"]) {
        read_to = DEFAULT_READ_TIMEOUT;
    } else {
        read_to = root["read_timeout"].asInt();
    }

    // alibaba cloud sdk init
    if (gtb_sdk_init(root["access_key_id"].asString(),
                     root["access_key_secret"].asString(),
                     conn_to,
                     read_to)) {
        cerr << "Init Alibaba cloud SDK failed." << endl;
        return -EBUSY;
    }

    // get domain name record
    string record_id, record_value, record_rr;
    if (gtb_get_domain_record(root["domain_name"].asString(),
                              &record_id,
                              &record_value,
                              &record_rr)) {
        cerr << "Invalid domain name." << endl;
        return -EINVAL;
    }
    cout << "Old IPv4:     [" << record_value << "]" << endl;

    // get current ipv4
    string ipv4;
    if (gtb_get_curr_ipv4(root["query_ipv4"].asString(), &ipv4)) {
        cerr << "Cannot get current IPv4." << endl;
        return -EBUSY;
    }
    cout << "Current IPv4: [" << ipv4 << "]" << endl;

    // check if needed to update record
    if (record_value == ipv4) {
        cout << "No need to update." << endl;
        return 0;
    }

    // set domain record
    if (gtb_set_domain_record(record_id,
                              record_rr,
                              ipv4)) {
        cerr << "Set domain record failed." << endl;
        return -EINVAL;
    }
    cout << "Record successfully updated." << endl;

    // shutdown alibaba cloud sdk
    gtb_sdk_shutdown();

    return 0;
}
/**********************************************************************************************************************
    end
**********************************************************************************************************************/
