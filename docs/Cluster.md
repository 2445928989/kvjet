每个节点维护两个线程，主线程和heartbeat线程，主线程用于收发gossip/指令/新连接，heartbeat线程用于收发心跳包、标记outtime。

当新节点接入时，首先接入者跟集群任意一个节点主线程建立socket，调用GETWORK获取完整的网络拓扑，接入者收到消息以后同步到一致性哈希，然后生成一个UUID，先发送一个HELLO指令，再发送一个gossip:NODEIN指令，收到gossip的集群节点检查一下这个节点是否已经存在，若存在就不连socket，把该节点加入自己的一致性哈希信息和网络拓扑信息，以SET的形式发kvstore里不属于自己的信息，转发gossip（包括客户端）

所以规范一下服务端的行为就是

接入：和集群建立sockt->发送GETNETWORK->接收网络拓扑->同步到一致性哈希和网络拓扑->生成UUID并发送HELLO->散播gossip:NODEIN->主线程epoll启动开始工作->接受到HELLO时将它加入集群连接信息中->返回OK

工作状态的节点：
收到GETNETWORK->发送网络拓扑
收到gossip:NODEIN->更新网络拓扑和一致性哈希->和新节点建立socket->发送HELLO告诉他自己是谁->遍历kvstore，检查哪些数据已经不归自己管辖，以指令SET的形式发送给新节点->转发gossip:NODEIN（不包括客户端）
收到gossip:NODEOUT->更新网络拓扑和一致性哈希->转发gossip:NODEOUT（不包括客户端）(不需要主动与OUT的NODE断开连接，它自己会断开)
收到HELLO:把这个socket扔进集群连接信息里
outtme的话暂不处理。

节点下线时，用NODEOUT命令gossip广播自己下线的消息（不包括客户端），收到gossip的人先更新拓扑，然后下线的节点遍历自己的kvstore，把数据发送给管辖对应数据的节点。

关于gossip的实现，对每个gossip维护一个唯一的UUID，每个服务端维护一个map，记录已收到的gossip的UUID，当收到一个gossip时，先检查UUID，如果已经收到过了就丢弃，否则处理并且转发给其他节点，如果map过大就丢掉最早来的gossip记录。

决定向外发gossip的时候，先自己记录一下它的UUID。不然会多次触发。

客户端无需转发gossip。

关于客户端的行为，当客户端接入集群某节点的时候应发送一个GETNETWORK指令，集群节点收到该指令应把完整的网络拓扑发给客户端，客户端和所有节点建立连接后开始正常工作。

如果客户端询问的某个key不在当前节点的管辖范围内（因为丢包之类的原因没有和集群同步拓扑结构），当前节点返回一个ERROR MOVED，客户端收到该错误后应向节点用GETNETWORK询问完整的网络拓扑结构，同步后再进行询问。

相关命令的封装：
HELLO (SIMPLESTRING)UUID
告诉你我是谁。返回OK
GETNETWORK
获取网络拓扑结构

gossip:NODEIN (SIMPLESTRING)gossipUUID (单个节点信息)
有节点来了
gossip:NODEOUT (SIMPLESTRING)gossipUUID (SIMPLESTRING)UUID
有节点下线

(ERROR)MOVED
这个数据已经迁移了不归我管

网络拓扑封装：
ARRAY套{(SIMPLESTRING)IP (INTEGER)PORT (SIMPLESTRING)UUID}
单个节点信息
ARRAY套{多个单个节点信息}
完整网络拓扑