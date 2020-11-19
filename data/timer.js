var connection = new WebSocket('ws://'+location.hostname+':81/', ['arduino']);
//var connection = new WebSocket('ws://192.168.2.123:81/');
var timer;
var remainingTime=0;
var timerTimeSec=0;
var timerRefreshEnd=0;

connection.onopen = function () {
    connection.send('Connect ' + new Date());
	
	// switch DieZeile to timer mode
	// send timer time = 0
	var sendStr = 'tt' + 0;
	console.log('Timer: ' + sendStr); 
	connection.send(sendStr);

	// start timer mode
	sendStr = 'M' + '10';    
	console.log('Timer start: ' + sendStr); 
	connection.send(sendStr);
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
    if(message.startsWith('v')) {
		message = message.slice('v'.length);
		document.getElementById('versionID').innerHTML = 'Version ' + message;
		}
    if(message.startsWith('Tg')) {
		message = message.slice('Tg'.length);
		remainingTime=message;
		console.log('Timer: ' + message + ' (remaining time)');
		setDigits(remainingTime);
		
		if(message=="0") {
			document.getElementById('timerStartStopID').innerHTML="DONE";			
			timerRefreshEnd=true;
		}
		if(timerRefreshEnd) {
			window.clearInterval(timer);
			timerRefreshEnd=false;
		};
	}
};

connection.onclose = function(){
    console.log('WebSocket connection closed');
    document.getElementById('statusID').innerHTML = "nicht verbunden";
	document.getElementById('statusID').style = "background-color: #CCCCCC";
};

// timer control --------------------------------------
// Button handler for increment and decrement buttons
function timerIncDec(obj, inc, max) {
	var oldVal = document.getElementById(obj).innerHTML;
	var newval = parseInt(oldVal) + inc;

	if(newval<0) 
		newval=max;
	if(newval>max) 
		newval=0;

	if(newval<10)
		newval = '0' + newval;

	console.log('timerIncDec: ' + obj + ' ' + oldVal + ' -> ' + newval.toString(10)); 

	document.getElementById(obj).innerHTML = newval;
	timerSetTime();
}

function timerSetTime() {
	document.getElementById('timerStartStopID').innerHTML="START";
	var hour   = document.getElementById('hourValueID').innerHTML;
	var minute = document.getElementById('minValueID').innerHTML;
	var second = document.getElementById('secValueID').innerHTML;
	timerTimeSec = parseInt(hour) * 3600 + parseInt(minute) * 60 + parseInt(second);

	// stop refresher
	window.clearInterval(timer);
		
	// set timer
	var sendStr = 'tt' + timerTimeSec;    
	console.log('Timer: ' + sendStr + ' (set time)'); 
	connection.send(sendStr);
}

// Button handler for START/STOP button
function timerStartPause() {
	var mode   = document.getElementById('timerStartStopID').innerHTML;
	var hour   = document.getElementById('hourValueID').innerHTML;
	var minute = document.getElementById('minValueID').innerHTML;
	var second = document.getElementById('secValueID').innerHTML;
	var sendStr;

	remainingTime = parseInt(hour) * 3600 + parseInt(minute) * 60 + parseInt(second);
	
    console.log('Timer: Button=' + mode); 

	if(mode=='DONE') {
		document.getElementById('timerStartStopID').innerHTML="START";
		var sendStr = 'ts0';    
		console.log('Timer: ' + sendStr + ' (reset)'); 
		connection.send(sendStr);

		refreshTimer(); // get original timer time		
	}
	else if(mode=='START') {
		document.getElementById('timerStartStopID').innerHTML="PAUSE";
		
		// start timer
		// start refresher
		timerRefreshEnd=false;
		timer=window.setInterval(refreshTimer, 1000);
		
		sendStr = 'ts1';    
		console.log('Timer: ' + sendStr + ' (start)'); 
		connection.send(sendStr);
	}
	else {
		document.getElementById('timerStartStopID').innerHTML="START";

		// pause timer
		// stop refresher
		timerRefreshEnd=true;
	
		sendStr = 'ts2';    
		console.log('Timer: ' + sendStr + ' (pause)'); 
		connection.send(sendStr);
	}
}

// Button handler for RESET button
function timerReset() {
	document.getElementById('timerStartStopID').innerHTML="START";
	setDigits(timerTimeSec);
	
	// stop refresher
	timerRefreshEnd=true;

	var sendStr = 'ts0';    
	console.log('Timer: ' + sendStr + ' (reset)'); 
	connection.send(sendStr);
}

// Button handler for CLEAR button
function timerClear() {
	document.getElementById('timerStartStopID').innerHTML="START";
	timerTimeSec=0;
	setDigits(timerTimeSec);
	
	// stop refresher
	timerRefreshEnd=true;

	var sendStr = 'tt0';    
	console.log('Timer: ' + sendStr + ' (set timer)'); 
	connection.send(sendStr);

	var sendStr = 'ts0';    
	console.log('Timer: ' + sendStr + ' (reset)'); 
	connection.send(sendStr);
}

// Button handler for EXIT button
function timerExit() {
	var sendStr = 'ts3';    
	console.log('Timer: ' + sendStr + ' (exit)'); 
	connection.send(sendStr);

	// stop refresher
	timerRefreshEnd=true;

	// display timer page
	window.location.href = 'index.html';
}

function refreshTimer() {
	//console.log('Remaining ' + hours + ':' + minutes + ':' + seconds); 			
	console.log('refreshTimer()'); 		
	sendStr = 'tg';    
	console.log('Timer: ' + sendStr + ' (get remaining time)'); 
	connection.send(sendStr);
}

function zeroPad(num, places) {
  var zero = places - num.toString().length + 1;
  return Array(+(zero > 0 && zero)).join("0") + num;
}

function setDigits(time) {
	var hours  =Math.floor(time/3600);
	var minutes=Math.floor((time-hours*3600)/60);
	var seconds=(time-hours*3600-minutes*60);

	document.getElementById('hourValueID').innerHTML=zeroPad(hours, 2);
	document.getElementById('minValueID').innerHTML=zeroPad(minutes, 2);
	document.getElementById('secValueID').innerHTML=zeroPad(seconds, 2);
}

