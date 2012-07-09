(function(window){

var defaults = {
	host: 'localhost',
	port: '8080',
	url: '/',
	
	connected: false,
	closed: false,
}

/* CrbObject */
function CrbObject() {
	if( typeof arguments[0] == 'object') {
		this.set(arguments[0]);
	}	
}
CrbObject.prototype = {
	set: function(data) {
		for( ind in data ) {
			this[ind] = data[ind];
		}
		return this;
	}
}
CrbObject.set = function ( target, data ) {
	for( ind in data ) {
		target[ind] = data[ind];
	}
	return target;
}
CrbObject.extend = function ( target, data ) {
	if( !target ) {
		target = {};
	};
	for( ind in data ) {
		if( typeof target[ind] == 'undefined' ){
			target[ind] = data[ind];
		};
	}
	return target;
}



/* Caribou */
function Caribou(options) {
	this.options = CrbObject.extend(options, defaults);
	this.socket = false;
	this.channels = {};
	CrbObject.call(this, arguments[0]);
	
	this.rx = {
		channel: /^[^\n]*/,
		message: /^[^\n]*\n/
	}
	
	this.connect();
}

Caribou.prototype = new CrbObject({
	constructor: Caribou,
	// methods
	connect: function(){
		var th = this;
		
		th.socket = new WebSocket('ws://' + th.options.host + ':' + th.options.port + th.options.url);
		
		this.socket.onopen = function(ev){
			th.onopen(ev);
		};
		this.socket.onclose = function(ev){
			th.onclose(ev);
		};
		this.socket.onerror = function(ev){
			th.onerror(ev);
		};
		this.socket.onmessage = function(ev){
			th.onmessage(ev);
		};
	},
	subscribe: function(channel){
		return this.createChannel(channel);
	},
	unsubscribe: function(channel){
		return this.removeChannel(channel);
	},
	
	// privite methods
	createChannel: function(name){
		var channel;
		
		if( typeof this.channels[name] != 'undefined' ){
			return this.channels[name];
		}; 
		
		channel = new CrbChannel(name);
		channel.socket = this.socket;
		
		try {
			this.socket.send('CTL\nSubscribe: ' + name + '\n')
		} catch (e) {
			console.log(e);
			return false;
		}
		
		this.channels[name] = channel;
		
		return channel;
	},
	removeChannel: function(name){
		var channel;
		
		if( typeof this.channels[name] == 'undefined' ){
			return;
		}; 
		
		channel = this.channels[name];
		
		try {
			channel.close();
		} catch (e) {
			return false;
		}
		
		delete this.channels[name];
	},
	
	// events
	onopen: function(e){
		if( this.connected ){
			this.connected();
		};
	},
	onclose: function(e){
		console.log('onclose', e);
	},
	onerror: function(e){
		console.log('onerror', e);
	},
	onmessage: function(e){
		var message, channel_name;
		
		channel_name = this.rx.channel.exec(e.data);
		if( !channel_name || !channel_name[0] ){
			return;
		};
		
		channel_name = channel_name[0];
		message = e.data.replace(this.rx.message, '');
		
		
		if( typeof this.channels[channel_name] == 'undefined' ){
			return;
		}; 
		
		this.channels[channel_name].onmessage(message);
	}
});

/* CrbChannel */
function CrbChannel(name) {
	this.socket = false;
	this.name = name;
	CrbObject.call(this, arguments[0]);
	
	this.listeners = []
}

CrbChannel.prototype = new CrbObject({
	constructor: CrbChannel,
	send: function( message ){
		return this.socket.send('DAT\n' + this.name + '\n' + JSON.stringify(message))
	},
	close: function( message ){
		return this.socket.send('CTL\nUnsubscribe: ' + name + '\n')
	},
	addListener: function(callback) {
		this.listeners.push(callback);
	},
	removeListener: function(callback) {
		var listeners =  this.listeners;
		for ( i=0; i < listeners.length; i++ ) {
			if ( listeners[i] === callback ) {
				delete listeners[i]
				break;
			}
		}
	},
	onmessage: function(data) {
		data = JSON.parse(data);
		var listeners =  this.listeners;
		for ( i=0; i < listeners.length; i++ ) {
			if ( listeners[i] && listeners[i].call(this, data) === false ) {
				break;
			}
		}
	}
});



window.Caribou = Caribou;

})(window);
