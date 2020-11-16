var connection = new WebSocket('ws://'+location.hostname+':81/', ['arduino']);
//var connection = new WebSocket('ws://192.168.2.123:81/');
var timer;
var remainingTime=0;

connection.onopen = function () {
    connection.send('Connect ' + new Date());

};

connection.onerror = function (error) {
    console.log('WebSocket Error ', error);
};

connection.onmessage = function (e) {  
    console.log('Server: ', e.data);
    var message = e.data;

    if(message.startsWith('C')) {
		message = message.slice('c'.length);
		document.getElementById('statusID').innerHTML = message;
		document.getElementById('statusID').style = "background-color: #00FF00";
	}
    if(message.startsWith('b')) {
		message = message.slice('b'.length);
		document.getElementById('brightnessID').value = message;
	}
    if(message.startsWith('SP')) {
		message = message.slice('SP'.length);
		document.getElementById('secretPeriodID').value = message;
		setSecretPeriod(message);
	}
    if(message.startsWith('SW')) {
		message = message.slice('SW'.length);
		document.getElementById('secretWindowID').value = message;
		setSecretWindow(message);
		}
    if(message.startsWith('v')) {
		message = message.slice('v'.length);
		document.getElementById('versionID').innerHTML = 'Version ' + message;
		}
    if(message.startsWith('m')) {
		message = message.slice('m'.length);
        document.getElementById('modeClockID').checked = false;
        document.getElementById('modeMathID').checked = false;
        document.getElementById('modeProgressID').checked = false;
        document.getElementById('modeTextID').checked = false;
        document.getElementById('modeTextClockID').checked = false;
        document.getElementById('modeSetTheoryClockID').checked = false;
        document.getElementById('modeFontClockID').checked = false;
        document.getElementById('modePercentClockID').checked = false;
		if(message == '1') {
			document.getElementById('modeClockID').checked = true;
		}
		if(message == '2') {
			document.getElementById('modeMathID').checked = true;
		}
		if(message == '4') {
			document.getElementById('modeProgressID').checked = true;
		}
		if(message == '5') {
			document.getElementById('modeTextID').checked = true;
		}
		if(message == '6') {
			document.getElementById('modeTextClockID').checked = true;
		}
		if(message == '7') {
			document.getElementById('modeSetTheoryClockID').checked = true;
		}
		if(message == '8') {
			document.getElementById('modeFontClockID').checked = true;
		}
		if(message == '9') {
			document.getElementById('modePercentClockID').checked = true;
		}
	}
    if(message.startsWith('cf')) {
		message = message.slice('cf'.length);
        document.getElementById('fontClassicID').checked = false;
        document.getElementById('fontBoldID').checked = false;
        document.getElementById('fontSmallID').checked = false;
		if(message == '0') {
			document.getElementById('fontSmallID').checked = true;
		}
		else if(message == '1') {
			document.getElementById('fontBoldID').checked = true;
		}
		else {
			document.getElementById('fontClassicID').checked = true;
		}
	}
    if(message.startsWith('cm')) {
		message = message.slice('cm'.length);
		if(message == '1') {
			document.getElementById('fontClockMonatID').checked = true;
		}
		else {
			document.getElementById('fontClockMonatID').checked = false;
		}
	}
    if(message.startsWith('cM')) {
		message = message.slice('cM'.length);
		if(message == '0') {
			document.getElementById('setMathModeAllID').checked = true;
		}
		else  {
			document.getElementById('setMathModeAddID').checked = true;
		}
	}
    if(message.startsWith('s')) {
		message = message.slice('s'.length);
		document.getElementById('speedID').value = message;
	}
    if(message.startsWith('t')) {
		message = message.slice('t'.length);
		document.getElementById('textID').value = message;
	}
    if(message.startsWith('i')) {
		message = message.slice('i'.length);
		document.getElementById('ssidID').value = message;
	}
    if(message.startsWith('p')) {
		message = message.slice('p'.length);
		document.getElementById('passwordID').value = message;
	}
};

connection.onclose = function(){
    console.log('WebSocket connection closed');
    document.getElementById('statusID').innerHTML = "nicht verbunden";
	document.getElementById('statusID').style = "background-color: #CCCCCC";
};

function sendBrightness() {
    var i = document.getElementById('brightnessID').value;
    var sendStr = 'B'+ i.toString(10);    
    console.log('Brightness: ' + sendStr); 
    connection.send(sendStr);
}

function sendSpeed() {
    var i = document.getElementById('speedID').value;
    var sendStr = 'S'+ i.toString(10);    

    console.log('Speed: ' + sendStr); 
    connection.send(sendStr);
}

function sendText() {
    var i = document.getElementById('textID').value;
    var sendStr = 'T'+ i;    
    console.log('Text: ' + sendStr); 
    connection.send(sendStr);
}

function sendOperationMode(mode) {
    var sendStr = 'M' + mode;    
    console.log('Mode: ' + sendStr); 
    connection.send(sendStr);
}

function sendDisplayMode(mode) {
    var sendStr = 'dm'+ mode;    
    console.log('Display: ' + sendStr); 
    connection.send(sendStr);
}

function sendDisplayPeriod() {
    var i = document.getElementById('periodID').value;
    var sendStr = 'dp'+ i.toString(10);    
    console.log('Display: ' + sendStr); 
    connection.send(sendStr);
}

// message queue control ------------------------
// addAt position text
function sendMQAddAt() {
    var text = document.getElementById('textID').value;
    var pos  = document.getElementById('positionID').value;
    var sendStr = 'A' + pos + '=' + text;    
    console.log('MessageQueue: ' + sendStr + ' (add text at)'); 
    connection.send(sendStr);
}

// delete at position
function sendMQDeleteAt() {
    var pos  = document.getElementById('positionID').value;
    var sendStr = 'D' + pos;    
    console.log('MessageQueue: ' + sendStr + ' (delete at)'); 
    connection.send(sendStr);
}

// set separator text
function sendMQD() {
    var delimiter = document.getElementById('textMQSID').value;
    var sendStr = 'a' + delimiter;    
    console.log('MessageQueue: ' + sendStr + ' (set delimiter)'); 
    connection.send(sendStr);
}

function setClockFont(font) {
    var sendStr = 'cf' + font;    
    console.log('Clock: ' + sendStr + ' (font)'); 
    connection.send(sendStr);
}

function setClockMathMode(mode) {
    var sendStr = 'cM' + mode;    
    console.log('Clock: ' + sendStr + ' (math mode)'); 
    connection.send(sendStr);
}

function setClockMonat() {
    var checked = document.getElementById('fontClockMonatID').checked;
    var sendStr;    
	if(checked)
		sendStr = 'cm1';
	else	
		sendStr = 'cm0';
    console.log('Clock: ' + sendStr + ' (month display)'); 
    connection.send(sendStr);
}

function sendSecretPeriodInSec() {
    var i = document.getElementById('secretPeriodID').value;
    var sendStr = 'sP'+ i.toString(10);    
    console.log('SecretPeriod: ' + sendStr); 
	setSecretPeriod(i.toString(10));
    connection.send(sendStr);
}

function sendSecretWindowInSec() {
    var i = document.getElementById('secretWindowID').value;
    var sendStr = 'sW'+ i.toString(10);    
    console.log('SecretWindow: ' + sendStr); 
	setSecretWindow(i.toString(10));
    connection.send(sendStr);	
}

function setSecretPeriod(period) {
	document.getElementById('secretPeriodValueID').innerHTML = 'Period [' + period + ' sec]';
}
function setSecretWindow(window) {
	document.getElementById('secretWindowValueID').innerHTML = 'Window [' + window + ' sec]';
}

function sendSSID() {
    var i = document.getElementById('ssidID').value;
    var sendStr = 'I'+ i;    
    console.log('Text: ' + sendStr); 
    connection.send(sendStr);
}

function sendPassword() {
    var i = document.getElementById('passwordID').value;
    var sendStr = 'P'+ i;    
    console.log('Text: ' + sendStr); 
    connection.send(sendStr);
}

function saveConfig(){
    var sendStr = 'X';    
    console.log('Text: ' + sendStr); 
    connection.send(sendStr);
}

function zeroPad(num, places) {
  var zero = places - num.toString().length + 1;
  return Array(+(zero > 0 && zero)).join("0") + num;
}


