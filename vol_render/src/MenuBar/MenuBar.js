import React from 'react'
import { Button, Navbar, Alignment, FileInput } from "@blueprintjs/core";
import './MenuBar.scss'

export default class MenuBar extends React.Component {
	render() {
		const { onFileOpen, onSaveMeshList, onUndo } = this.props

		return (
			<div className="menuBar">
				<Navbar>
					<Navbar.Group align={Alignment.LEFT}>
						<Navbar.Heading>Toolbar</Navbar.Heading>
						<Navbar.Divider />
						<FileInput 
							text="Upload STL:"
							accept=".stl, .raw"
							onInputChange={onFileOpen}
						/>
						<Navbar.Divider />
						<Button 
							className="bp3-minimal" 
							icon="floppy-disk" 
							text="Save" 
							onClick={onSaveMeshList}
						/>
						<Button 
							className="bp3-minimal"
							icon="undo"
							text="Undo"
							onClick={onUndo}
						/>
					</Navbar.Group>
				</Navbar>
			</div>
		)
	}
}