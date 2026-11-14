<?php
// SPDX-License-Identifier: MIT
// Small, bounded SMTP submission transport; no local sendmail or shell commands.
declare(strict_types=1);
function smtp_settings(array $config): ?array {
    $s=$config['smtp']??null;
    if(!is_array($s) || empty($s['host']))return null;
    $s+=['port'=>587,'encryption'=>'starttls','username'=>'','password'=>'','from_address'=>'','from_name'=>'LineClash','hello_host'=>'localhost','source_ip'=>'','ca_file'=>'','timeout'=>10];
    if(!is_string($s['host']) || !preg_match('/^[a-zA-Z0-9.:-]{1,253}$/D',$s['host']) ||
       !is_int($s['port']) || $s['port']<1 || $s['port']>65535 ||
       !in_array($s['encryption'],['none','starttls','tls'],true) ||
       !is_string($s['from_address']) || !filter_var($s['from_address'],FILTER_VALIDATE_EMAIL) || preg_match('/[^\x21-\x7e]/',$s['from_address']) ||
       !is_string($s['hello_host']) || !preg_match('/^[a-zA-Z0-9.-]{1,253}$/D',$s['hello_host']) ||
       !is_string($s['from_name']) || strlen($s['from_name'])>45 || preg_match('/[\r\n\x00]/',$s['from_name']) ||
       !is_string($s['username']) || !is_string($s['password']) || preg_match('/[\r\n\x00]/',$s['username'].$s['password']) ||
       strlen($s['username'])>512 || strlen($s['password'])>512 ||
       !is_string($s['source_ip']) || ($s['source_ip']!=='' && !filter_var($s['source_ip'],FILTER_VALIDATE_IP)) ||
       !is_string($s['ca_file']) || ($s['ca_file']!=='' && !is_readable($s['ca_file'])) || !is_int($s['timeout']) || $s['timeout']<1 || $s['timeout']>30)return null;
    if($s['encryption']!=='none' && !extension_loaded('openssl'))return null;
    return $s;
}
function smtp_response($socket,array $expected): string {
    $response='';$code=null;
    for($i=0;$i<64;$i++){
        $line=fgets($socket,1025);
        if($line===false || strlen($line)>1000 || !preg_match('/^(\d{3})([ -])[^\r\n]*\r\n$/D',$line,$m))throw new RuntimeException('SMTP invalid response or timeout');
        if($code!==null && $code!==$m[1])throw new RuntimeException('SMTP inconsistent response');
        $code=$m[1];$response.=$line;
        if($m[2]===' '){if(!in_array((int)$code,$expected,true))throw new RuntimeException('SMTP rejected command ('.$code.')');return $response;}
    }
    throw new RuntimeException('SMTP response limit exceeded');
}
function smtp_write($socket,string $data): void {
    while($data!==''){$n=fwrite($socket,$data);if(!$n)throw new RuntimeException('SMTP write failed');$data=substr($data,$n);}
}
function smtp_command($socket,string $command,array $expected): string {
    smtp_write($socket,$command."\r\n");return smtp_response($socket,$expected);
}
function smtp_send(array $s,string $recipient,string $subject,string $body): void {
    if(!filter_var($recipient,FILTER_VALIDATE_EMAIL) || preg_match('/[^\x21-\x7e]/',$recipient) || preg_match('/[\r\n\x00]/',$subject))throw new RuntimeException('Invalid mail headers');
    $tls=['verify_peer'=>true,'verify_peer_name'=>true,'peer_name'=>$s['host'],'allow_self_signed'=>false,
          'crypto_method'=>STREAM_CRYPTO_METHOD_TLSv1_2_CLIENT|STREAM_CRYPTO_METHOD_TLSv1_3_CLIENT];
    if($s['ca_file']!=='')$tls['cafile']=$s['ca_file'];
    $options=['ssl'=>$tls];
    if($s['source_ip']!=='')$options['socket']['bindto']=(str_contains($s['source_ip'],':')?'['.$s['source_ip'].']':$s['source_ip']).':0';
    $host=str_contains($s['host'],':')?'['.$s['host'].']':$s['host'];
    $socket=@stream_socket_client(($s['encryption']==='tls'?'tls':'tcp').'://'.$host.':'.$s['port'],$errno,$error,$s['timeout'],STREAM_CLIENT_CONNECT,stream_context_create($options));
    if(!$socket)throw new RuntimeException('SMTP connection or TLS verification failed');
    stream_set_timeout($socket,$s['timeout']);
    try {
        smtp_response($socket,[220]);$hello=smtp_command($socket,'EHLO '.$s['hello_host'],[250]);
        if($s['encryption']==='starttls'){
            if(!preg_match('/^250[ -]STARTTLS\b/im',$hello))throw new RuntimeException('SMTP server does not offer STARTTLS');
            smtp_command($socket,'STARTTLS',[220]);
            if(@stream_socket_enable_crypto($socket,true,$tls['crypto_method'])!==true)throw new RuntimeException('SMTP STARTTLS verification failed');
            $hello=smtp_command($socket,'EHLO '.$s['hello_host'],[250]);
        }
        if($s['username']!==''){
            if(preg_match('/^250[ -]AUTH[ =][^\r\n]*\bPLAIN\b/im',$hello))smtp_command($socket,'AUTH PLAIN '.base64_encode("\0".$s['username']."\0".$s['password']),[235]);
            elseif(preg_match('/^250[ -]AUTH[ =][^\r\n]*\bLOGIN\b/im',$hello)){
                smtp_command($socket,'AUTH LOGIN',[334]);smtp_command($socket,base64_encode($s['username']),[334]);smtp_command($socket,base64_encode($s['password']),[235]);
            }else throw new RuntimeException('SMTP server does not offer supported authentication');
        }
        smtp_command($socket,'MAIL FROM:<'.$s['from_address'].'>',[250]);smtp_command($socket,'RCPT TO:<'.$recipient.'>',[250,251]);smtp_command($socket,'DATA',[354]);
        $message='Date: '.gmdate('D, d M Y H:i:s').' +0000'."\r\n".
            'Message-ID: <'.bin2hex(random_bytes(16)).'@'.$s['hello_host'].">\r\n".
            'From: =?UTF-8?B?'.base64_encode($s['from_name']).'?= <'.$s['from_address'].">\r\n".
            'To: <'.$recipient.">\r\n".'Subject: =?UTF-8?B?'.base64_encode($subject)."?=\r\n".
            "MIME-Version: 1.0\r\nContent-Type: text/plain; charset=UTF-8\r\nContent-Transfer-Encoding: base64\r\n\r\n".
            chunk_split(base64_encode($body),76,"\r\n");
        smtp_write($socket,$message.".\r\n");smtp_response($socket,[250]);
        // Delivery was accepted; a failed QUIT must not invalidate its reset link.
        try{smtp_command($socket,'QUIT',[221]);}catch(Throwable $ignored){}
    }finally{fclose($socket);}
}

function account_mail_settings(): ?array {
    $path=web_config_path();if(!is_file($path))return null;
    $config=require $path;if(!is_array($config))return null;$smtp=smtp_settings($config);
    $url=$config['public_url']??'https://lineclash.kladhest.se';
    if(!$smtp || !is_string($url) || strlen($url)>512 || !filter_var($url,FILTER_VALIDATE_URL))return null;
    $parts=parse_url($url);
    if(($parts['scheme']??'')!=='https' || isset($parts['user']) || isset($parts['pass']) || isset($parts['query']) || isset($parts['fragment']))return null;
    return ['smtp'=>$smtp,'url'=>rtrim($url,'/')];
}
