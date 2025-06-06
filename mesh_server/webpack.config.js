const path = require("path");
const TerserPlugin = require("terser-webpack-plugin");
const glob = require("glob-all");
module.exports = {
  entry: glob.sync("./src/**/*.js"),
  output: {
    filename: "main.js",
    path: path.resolve(__dirname, "dist"),
    library: "main",
    libraryTarget: "umd",
  },
  optimization: {
    minimize: true,
    minimizer: [
      new TerserPlugin({
        terserOptions: {
          format: {
            semicolons: false,
          },
        },
      }),
    ],
  },
  devServer: {
    static: {
      directory: path.join(__dirname, './')
    },
    open: true,
    hot: true,
    port: 9005
  },
};
