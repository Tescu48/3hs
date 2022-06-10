<!DOCTYPE html>
<html>
	<head>
		<meta charset="utf-8"/>
		<title>hLink | add to queue</title>
	</head>
	<body>
		[[if is-success?()]]
			<p>Added title [title-name] (with hShop ID [title-hshop-id]) to the queue.</p>
		[[else]]
			<p>An error occured: [error-message].</p>
		[[end]]
		<a href="/index.html">Back to home</a>
	</body>
</html>
