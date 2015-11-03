exports.FAILED = "FAILED";
exports.SUCCESS = "SUCCESS";

exports.send = function( event, context, status, data ) {
	var response = JSON.stringify( {
		Status : status,
		StackId : event.StackId,
		RequestId : event.RequestId,
		LogicalResourceId : event.LogicalResourceId,
		PhysicalResourceId : context.logStreamName,
		Data : data
	} );

	var url = require( "url" );
	var https = require( "https" );

	var parsed = url.parse( event.ResponseURL );
	var options = {
		hostname : parsed.hostname,
		port : 443,
		path : parsed.path,
		method : "PUT",
		headers : {
			"content-length" : response.length
		}
	};

	var request = https.request( options, function() {
		context.done();
	} );

	request.on( "error", function( error ) {
		console.log( error );
		context.done();
	} );

	request.write( response );
	request.end();
};
