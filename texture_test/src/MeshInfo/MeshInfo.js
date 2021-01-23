import React from 'react'

import { Card, Elevation} from "@blueprintjs/core"

import './MeshInfo.scss'

export default class MeshInfo extends React.Component {
	render() {
		const { meshName, meshPositionX, meshPositionY, meshPositionZ, 
			meshRotationX, meshRotationY, meshRotationZ, onPositionXChange,
			onPositionYChange, onPositionZChange, onRoatationXChange,
			onRoatationYChange, onRoatationZChange} = this.props

		return (
			<div id="meshInfo" className="">
				<Card interactive={true} elevation={Elevation.TWO}>
					<h4>{meshName}</h4>
					<div className="meshInfoGrid">
						<div className="meshInfoPositionHeader">Position</div>
						<div className="meshInfoRotationHeader">Rotation</div>
						<div className="meshInfoXLabel">X:</div>
						<div className="meshInfoYLabel">Y:</div>
						<div className="meshInfoZLabel">Z:</div>
						<div className="meshInfoXPositionInput">
							<input
								id="xPositionInput"
								value={meshPositionX}
								onChange={onPositionXChange}
							/>
						</div>
						<div className="meshInfoYPositionInput">
							<input
								id="yPositionInput"
								value={meshPositionY}
								onChange={onPositionYChange}
							/>
						</div>
						<div className="meshInfoZPositionInput">
							<input
								id="zPositionInput"
								value={meshPositionZ}
								onChange={onPositionZChange}
							/>
						</div>
						<div className="meshInfoXRotationInput">
							<input
								id="xRotationInput"
								value={meshRotationX}
								onChange={onRoatationXChange}
							/>
						</div>
						<div className="meshInfoYRotationInput">
							<input
								id="yRotationInput"
								value={meshRotationY}
								onChange={onRoatationYChange}
							/>
						</div>
						<div className="meshInfoZRotationInput">
							<input
								id="zRotationInput"
								value={meshRotationZ}
								onChange={onRoatationZChange}
							/>
						</div>
					</div>
				</Card>
			</div>



		)
	}
}



// <div id="meshInfo" className="">
// <div className="meshInfoName" id="meshName">
// 	{meshName}
// </div>

