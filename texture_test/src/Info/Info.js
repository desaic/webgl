import React from 'react'
import './Info.scss'
import { Callout } from "@blueprintjs/core";

export default class Info extends React.Component {
	render() {
		const { boundaryExceded } = this.props

		return (
			<>
				<div className="info">
					<Callout >
						"T" translate | "R" rotate | "S" scale | "Shift" snap to grid
						<br />
						"DEL" delete selected object | "ESC" deselect | "D" duplicate
					</Callout>
					{/* <div class="bp3-callout .modifier">
						<h4 class="bp3-heading">Shortcuts</h4>
						"T" translate | "R" rotate | "S" scale | "Shift" snap to grid
						<br />
						"DEL" delete selected object | "ESC" deselect | "D" duplicate
					</div> */}
				</div>
				{boundaryExceded && (

					<div className="boundaryErrorNotification">
						<Callout title="Warning" intent={"warning"}>
							One of more mesh objects are outside the permitted boundary
						</Callout>
					</div>
				)}
			</>
		)
	}
}