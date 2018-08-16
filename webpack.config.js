const path = require('path');

module.exports = {
    mode: 'development',
    entry: {
        main: './webpack/entry',
        cells: './webpack/cells'
    },
    output: {
        path: path.resolve(__dirname, 'src', 'assets', 'js'),
        filename: 'bundle--[name].js'
    },
    resolve: {
        extensions: [".webpack.js", ".web.js", ".ts", ".tsx", ".js"]
    },
    module: {
        rules: [
            {
                test: /\.tsx?$/,
                loader: "ts-loader"
            },
        ]
    }
}