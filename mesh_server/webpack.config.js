const path = require('path');
const TerserPlugin = require('terser-webpack-plugin');
module.exports = {
  entry: './src/index.js',
  output: {
    filename: 'main.js',
    path: path.resolve(__dirname, 'dist'),
  },  optimization: {
  minimize: true,
  minimizer: [ new TerserPlugin({
            terserOptions: {
                format: {
                    semicolons : false
                }
            }
        })]
  }
};