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
            />
        </div>

        <div className="VarLabel">X:</div>
        <div className ="VarInput">
            <input 
                id="InputX"
                value={GetVar(1)}
            />
        </div>

        <div className="VarLabel">Y:</div>
        <div className ="VarInput">
            <input 
                id="InputY"
                value={GetVar(2)}
            />
        </div>

        <div className="VarLabel">3:</div>
        <div className ="VarInput">
            <input 
                id="InputY"
                value={GetVar(3)}
            />
        </div>

        <div className="VarLabel">4:</div>
        <div className ="VarInput">
            <input 
                id="InputY"
                value={GetVar(4)}
            />
        </div>
        <div className="VarLabel">5:</div>
        <div className ="VarInput">
            <input 
                id="InputY"
                value={GetVar(5)}
            />
        </div>
        <div className="VarLabel">6:</div>
        <div className ="VarInput">
            <input 
                id="InputY"
                value={GetVar(6)}
            />
        </div>

    </div>
</div>)
}
}