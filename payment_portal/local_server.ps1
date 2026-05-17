$listener = New-Object System.Net.HttpListener
$listener.Prefixes.Add("http://localhost:8888/")
try {
    $listener.Start()
    Write-Output "PowerShell Micro-Server running on http://localhost:8888/"
    while ($listener.IsListening) {
        $context = $listener.GetContext()
        $request = $context.Request
        $response = $context.Response
        
        $url = $request.Url.ToString()
        if ($url -like "*success*") {
            $query = $request.QueryString
            $user = $query["user"]
            $gold = $query["gold"]
            $code = $query["code"]
            
            if ($user -and $gold -and $code) {
                $payload = @{
                    username = $user
                    gold = [int]$gold
                    code = $code
                    timestamp = (Get-Date).ToString("o")
                } | ConvertTo-Json
                
                Set-Content -Path "c:\Users\DELL\source\repos\Project\RAS Game\payment_success.json" -Value $payload
                Write-Output "Received payment notification: $gold Gold for $user ($code). Generated payment_success.json."
            }
        }
        
        $response.Headers.Add("Access-Control-Allow-Origin", "*")
        $response.StatusCode = 200
        $buffer = [System.Text.Encoding]::UTF8.GetBytes("OK")
        $response.ContentLength64 = $buffer.Length
        $response.OutputStream.Write($buffer, 0, $buffer.Length)
        $response.Close()
    }
} catch {
    Write-Error $_
} finally {
    $listener.Stop()
}
