#!/bin/bash

find . -type f \( -name "*.cpp" -o -name "*.hpp" \) \( -path "./RAT-Server/*" -o -path "./RAT-Client/*" -o -path "./Utils/*" \) -exec sed -i \
-e 's/\blistener_\b/listener/g' \
-e 's/\bport_\b/port/g' \
-e 's/\brunning_\b/running/g' \
-e 's/\bmtx_\b/mtx/g' \
-e 's/\bclientManagement_\b/clientManagement/g' \
-e 's/\bcontrollers_\b/controllers/g' \
-e 's/\bsocket_\b/socket/g' \
-e 's/\bmanager_\b/manager/g' \
-e 's/\budpSocket_\b/udpSocket/g' \
-e 's/\budpThread_\b/udpThread/g' \
-e 's/\bstdinThread_\b/stdinThread/g' \
-e 's/\bselectedClient_\b/selectedClient/g' \
-e 's/\bbashResponses_\b/bashResponses/g' \
-e 's/\bbashMtx_\b/bashMtx/g' \
-e 's/\bpendingResponse_\b/pendingResponse/g' \
-e 's/\bresponseMtx_\b/responseMtx/g' \
-e 's/\bresponseCv_\b/responseCv/g' \
-e 's/\bclients_\b/clients/g' \
-e 's/\bclientIPs_\b/clientIPs/g' \
-e 's/\blogs_\b/logs/g' \
-e 's/\blogMtx_\b/logMtx/g' \
-e 's/\blogPath_\b/logPath/g' \
{} +
