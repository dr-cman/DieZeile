var connection = new WebSocket('ws://'+location.hostname+':81/', ['arduino']);

connection.onopen = function () {
    connection.send('Connect ' + new Date());
};

connection.onerror = function (error) {
    console.log('WebSocket Error ', error);
};

connection.onmessage = function (e) {  
    console.log('Server: ', e.data);
    var message = e.data;

    if(message.startsWith('c')) {
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
	}
    if(message.startsWith('SW')) {
		message = message.slice('SW'.length);
		document.getElementById('secretWindowID').value = message;
	}
    if(message.startsWith('m')) {
		message = message.slice('m'.length);
        document.getElementById('modeClockID').checked = false;
        document.getElementById('modeMathID').checked = false;
        document.getElementById('modeAddOnlyID').checked = false;
        document.getElementById('modeProgressID').checked = false;
        document.getElementById('modeTextID').checked = false;
		if(message == '1') {
			document.getElementById('modeClockID').checked = true;
		}
		if(message == '2') {
			document.getElementById('modeMathID').checked = true;
		}
		if(message == '3') {
			document.getElementById('modeAddOnlyID').checked = true;
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
	}
    if(message.startsWith('f')) {
		message = message.slice('f'.length);
        document.getElementById('fontClassicID').checked = false;
        document.getElementById('fontBoldID').checked = false;
        document.getElementById('fontSmallID').checked = false;
		if(message == '0') {
			document.getElementById('fontSmallID').checked = true;
		}
		if(message == '1') {
			document.getElementById('fontBoldID').checked = true;
		}
		if(message == '2') {
			document.getElementById('fontClassicID').checked = true;
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

function sendModeClock() {
    var sendStr = 'M' + '1';    
    console.log('Mode: ' + sendStr); 
    connection.send(sendStr);
}

function sendModeMath() {
    var sendStr = 'M' + '2';    
    console.log('Mode: ' + sendStr); 
    connection.send(sendStr);
}

function sendModeAddOnly() {
    var sendStr = 'M' + '3';    
    console.log('Mode: ' + sendStr); 
    connection.send(sendStr);
}

function sendModeProgress() {
    var sendStr = 'M' + '4';    
    console.log('Mode: ' + sendStr); 
    connection.send(sendStr);
}

function sendModeTextClock() {
    var sendStr = 'M' + '6';    
    console.log('Mode: ' + sendStr); 
    connection.send(sendStr);
}

function sendModeSetTheoryClock() {
    var sendStr = 'M' + '7';    
    console.log('Mode: ' + sendStr); 
    connection.send(sendStr);
}

function sendModeFontClock() {
    var sendStr = 'M' + '8';    
    console.log('Mode: ' + sendStr); 
    connection.send(sendStr);
}

function sendModeText() {
    var sendStr = 'M' + '5';    
    console.log('Mode: ' + sendStr); 
    connection.send(sendStr);
}

function setFontSmall() {
    var sendStr = 'f' + '0';    
    console.log('Font: ' + sendStr); 
    connection.send(sendStr);
}

function setFontBold() {
    var sendStr = 'f' + '1';    
    console.log('Font: ' + sendStr); 
    connection.send(sendStr);
}

function setFontClassic() {
    var sendStr = 'f' + '2';    
    console.log('Font: ' + sendStr); 
    connection.send(sendStr);
}

function sendSectretPeriodInSec() {
    var i = document.getElementById('secretPeriodID').value;
    var sendStr = 'sP'+ i.toString(10);    
    console.log('SecretPeriod: ' + sendStr); 
    connection.send(sendStr);
}

function sendSectretWindowInSec() {
    var i = document.getElementById('secretWindowID').value;
    var sendStr = 'sW'+ i.toString(10);    
    console.log('SecretWindow: ' + sendStr); 
    connection.send(sendStr);
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
