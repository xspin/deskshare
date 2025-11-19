
#!/bin/bash

APP_NAME="deskshare"

# 创建目录结构
mkdir -p $APP_NAME.app/Contents/{MacOS,Resources}

# 复制可执行文件
cp -v build/bin/$APP_NAME $APP_NAME.app/Contents/MacOS/

# 生成 Info.plist
cat > $APP_NAME.app/Contents/Info.plist << EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key>
    <string>$APP_NAME</string>
    <key>CFBundleExecutable</key>
    <string>$APP_NAME</string>
    <key>CFBundleIdentifier</key>
    <string>com.example.$APP_NAME</string>
    <key>CFBundleVersion</key>
    <string>1.0</string>
    <key>LSUIElement</key>
    <true/>
</dict>
</plist>
EOF

# 赋予权限
chmod +x $APP_NAME.app/Contents/MacOS/$APP_NAME

echo "Generated: $APP_NAME.app"