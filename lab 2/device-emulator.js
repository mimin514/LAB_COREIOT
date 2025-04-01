var mqtt = require('mqtt');

const thingsboardHost = process.argv[2];
const accessToken = process.argv[3];

// Default topics. See http://thingsboard.io/docs/reference/mqtt-api/ for more details.
const attributesTopic = 'v1/devices/me/attributes',
      telemetryTopic = 'v1/devices/me/telemetry',
      attributesRequestTopic = 'v1/devices/me/attributes/request/1',
      attributesResponseTopic = attributesRequestTopic.replace('request', 'response'),
      rpcRequestTopic = 'v1/devices/me/rpc/request/',
      rpcResponseTopic = 'v1/devices/me/rpc/response/';

// Initialization of mqtt client using Thingsboard host and device access token
console.log('Connecting to: %s using access token: %s', thingsboardHost, accessToken);
var client = mqtt.connect('mqtt://' + thingsboardHost, {username: accessToken});

var currentState = 'Off',
    currentTemperature = +(Math.random() * 5 + 25).toFixed(1),
    currentFirmwareVersion = 'v2.0';

// Triggers when client is successfully connected to the Thingsboard server
client.on('connect', function () {
    console.log('Client connected!');

    client.subscribe(attributesTopic);
    client.subscribe(attributesResponseTopic);
    client.subscribe(rpcRequestTopic + '+');

    client.publish(attributesRequestTopic, JSON.stringify({
        'sharedKeys': 'targetState,targetTemperature,targetFirmwareVersion'
    }));

    setInterval(publishPowerConsumption, 5000);
});
function reportCurrentStatus() {
    setTimeout(() => {
        var status = JSON.stringify({
            'currentState': currentState, 
            'currentTemperature': currentTemperature, 
            'currentFirmwareVersion': currentFirmwareVersion
        });
        console.log('Reporting current status: %s', status);
        client.publish(telemetryTopic, status);
    }, 1000);  // Chờ 1 giây trước khi gửi
}

client.on('message', function (topic, message) {
    if (topic ===  attributesResponseTopic) {
        // Handles response to attributes request
        console.log('[%s] Received response to attribute request: %s', getTime(), message.toString());
        var data = JSON.parse(message);
        if (!data.shared){
            return;
        }
        if (data.shared.targetState) {
            currentState = data.shared.targetState;
        }
        if (data.shared.targetTemperature) {
            currentTemperature = data.shared.targetTemperature;
        }
        if (data.shared.targetFirmwareVersion) {
            currentFirmwareVersion = data.shared.targetFirmwareVersion;
        }
        reportCurrentStatus();
    } else if (topic === attributesTopic) {
        // Handles attributes update notification
        console.log('[%s] Received attribute update notification: %s', getTime(), message.toString());
        var data = JSON.parse(message);
        if (data.targetState && data.targetState != currentState) {
            currentState = data.targetState;
            reportCurrentStatus();
        }
        if (data.targetTemperature && data.targetTemperature != currentTemperature) {
            currentTemperature = data.targetTemperature;
            reportCurrentStatus();
        }
        if (data.targetFirmwareVersion && data.targetFirmwareVersion != currentFirmwareVersion) {
            currentFirmwareVersion = data.targetFirmwareVersion;
            reportCurrentStatus();
        }
    } else if (topic.startsWith(rpcRequestTopic)) {
        // Handles RPC request
        console.log('[%s] Received RPC request: %s', getTime(), message.toString());
        var messageData = JSON.parse(message.toString());
        if (messageData.method === 'reportStatus') {
            reportCurrentStatus();
        }
    }
});

function publishPowerConsumption() {
    var power = currentState === "On" ? (+(Math.random() * 5 + 5).toFixed(1)) : 0.1;
    var powerConsumption = JSON.stringify({powerConsumption: power});
    client.publish('v1/devices/me/telemetry', powerConsumption);
}

function reportUpdate(updateJson){
    var update = JSON.stringify(updateJson);
    console.log('Reporting update: %s', update);
    client.publish(telemetryTopic, update);
}



function getTime(){
    return new Date().toLocaleTimeString();
}

// Catches ctrl+c event
process.on('SIGINT', function () {
    console.log();
    console.log('Disconnecting...');
    client.end();
    console.log('Exited!');
    process.exit(2);
});

// Catches uncaught exceptions
process.on('uncaughtException', function (e) {
    console.log('Uncaught Exception...');
    console.log(e.stack);
    process.exit(99);
});