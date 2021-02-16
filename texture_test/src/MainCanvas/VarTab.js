import React from 'react'

import './VarTab.scss'

export default class VarTab extends React.Component {
render() {
    const { varList } = this.props;
    function GetVar  (i){
        if(i>=varList.length){
            return 0;
        }
        return varList[i];
    }
return (<div className="VarTable">
    <div className="VarGrid">
        <div className="VarLabel">A:</div>
        <div className ="VarInput">
            <input 
                id="InputA"
                value={GetVar(0)}
                readOnly
            />
        </div>

        <div className="VarLabel">X:</div>
        <div className ="VarInput">
            <input 
                id="InputX"
                value={GetVar(1)}
                readOnly
            />
        </div>

        <div className="VarLabel">Y:</div>
        <div className ="VarInput">
            <input 
                id="InputY"
                value={GetVar(2)}
                readOnly
            />
        </div>

        <div className="VarLabel">PC</div>
        <div className ="VarInput">
            <input 
                id="InputIP"
                value={GetVar(3)}
                readOnly
            />
        </div>

        <div className="VarLabel">SP:</div>
        <div className ="VarInput">
            <input 
                id="InputSP"
                value={GetVar(4)}
                readOnly
            />
        </div>
        <div className="VarLabel">BP:</div>
        <div className ="VarInput">
            <input 
                id="InputBP"
                value={GetVar(5)}
                readOnly
            />
        </div>
        <div className="VarLabel">INS:</div>
        <div className ="VarInput">
            <input 
                id="InputINS"
                value={GetVar(6)}
                readOnly
            />
        </div>
        <div className="VarLabel">INS1:</div>
        <div className ="VarInput">
            <input 
                id="InputINS1"
                value={GetVar(7)}
                readOnly
            />
        </div>
    </div>
</div>)
}
}