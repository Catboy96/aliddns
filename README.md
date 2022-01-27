# aliddns
Simple DDNS utility with Alibaba Cloud (Aliyun) SDK

## Usage
1. Create `.ddns_config` file in home directory, with following content:
```json
{
	"access_key_id" : "<AccessKey ID>",
	"access_key_secret" : "<AccessKey Secret>",
	"connect_timeout" : 1500,
	"read_timeout" : 4000,
	"domain_name" : "ddns.domain.com",
	"query_ipv4" : "https://api-ipv4.ip.sb/ip"
}
```
* `access_key_id`: Aliyun AccessKey ID
* `access_key_secret`: Aliyun AccessKey Secret
* `connect_timeout` (optional): Aliyun SDK connection timeout in ms
* `read_timeout` (optional): Aliyun SDK read timeout in ms
* `domain_name`: Domain name to update A record
* `query_ipv4`: URL to query current public IPv4 address, **this URL should return IPv4 plain text!!!**

2. Simply run this to check and update domain record if needed:
```shell
aliddns
```

## Dependencies
* curl
* jsoncpp
* alibabacloud-sdk-alidns
* alibabacloud-sdk-core

[Install Alibaba Cloud SDK](https://github.com/aliyun/aliyun-openapi-cpp-sdk/blob/master/README-CN.md)  

When installing Alibaba Cloud SDK, these parts should be compiled and installed: `core` and `alidns`.  
Use following commands:  
```shell
sudo sh easyinstall.sh core
sudo sh easyinstall.sh alidns
```

