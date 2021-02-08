import React from 'react'


import { Card, Elevation} from "@blueprintjs/core"

import './MeshInfo.scss'

export default class MeshInfo extends React.Component {
	render() {
		///\param onMeshTrans callback for when mesh transformation changes
		///from the mesh info ui.
		const { meshName, meshTrans, onMeshTrans} = this.props

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
								value={meshTrans.pos[0]}
								onChange={ (e)=>onMeshTrans(e, 0) }
							/>
						</div>
						<div className="meshInfoYPositionInput">
							<input
								id="yPositionInput"
								value={meshTrans.pos[1]}
								onChange={ (e)=>onMeshTrans(e, 1) }
							/>
						</div>
						<div className="meshInfoZPositionInput">
							<input
								id="zPositionInput"
								value={meshTrans.pos[2]}
								onChange={ (e)=>onMeshTrans(e, 2) }
							/>
						</div>
						<div className="meshInfoXRotationInput">
							<input
								id="xRotationInput"
								value={meshTrans.rot[0]}
								onChange={ (e)=>onMeshTrans(e, 3) }
							/>
						</div>
						<div className="meshInfoYRotationInput">
							<input
								id="yRotationInput"
								value={meshTrans.rot[1]}
								onChange={ (e)=>onMeshTrans(e, 4) }
							/>
						</div>
						<div className="meshInfoZRotationInput">
							<input
								id="zRotationInput"
								value={meshTrans.rot[2]}
								onChange={ (e)=>onMeshTrans(e, 5) }
							/>
						</div>
					</div>
				</Card>
			</div>



		)
	}
}
