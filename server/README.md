1. compile Javascript file containting JSX syntax using Babel
Reference: https://reactjs.org/docs/add-react-to-a-website.html
In command line, go to a build directory, the example uses
>mkdir build-react

>cd build-react

>npm init -y

>npm install babel-cli@6 babel-preset-react-app@3

>cp -r ../webgl/server/src ./

>npx babel src --out-dir ../webgl/server/ --presets react-app/prod

>cd ../webgl/server/

>node helloworld.js

In a webbrowser go to localhost:9000
